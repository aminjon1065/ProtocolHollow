#include "Sector/HollowSectorLoaderComponent.h"

#include "Assembly/HollowSectorAssemblySubsystem.h"
#include "Assembly/HollowSectorStateComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Framework/HollowPlayerController.h"
#include "HollowGameLog.h"
#include "Sector/HollowSectorFeatureSubsystem.h"
#include "Sector/HollowSectorSettings.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowSectorLoaderComponent)

UHollowSectorLoaderComponent::UHollowSectorLoaderComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHollowSectorLoaderComponent::BeginPlay()
{
	Super::BeginPlay();

	UHollowSectorStateComponent* State = GetOwner()->FindComponentByClass<UHollowSectorStateComponent>();
	UHollowSectorAssemblySubsystem* Assembly = GetWorld()->GetSubsystem<UHollowSectorAssemblySubsystem>();
	if (!State || !Assembly)
	{
		return;   // not a game world (editor preview) or a game state without a sector
	}
	ManifestHandle = State->OnManifestChanged.AddUObject(this, &ThisClass::HandleManifest);
	AssembledHandle = Assembly->OnAssembled.AddUObject(this, &ThisClass::HandleAssembled);
	if (State->GetManifest().IsSet())
	{
		HandleManifest(State->GetManifest());   // replicated before this BeginPlay
	}
}

void UHollowSectorLoaderComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UHollowSectorStateComponent* State = GetOwner()->FindComponentByClass<UHollowSectorStateComponent>())
	{
		State->OnManifestChanged.Remove(ManifestHandle);
	}
	// Also reached from garbage collection of a torn-down world (a component's BeginDestroy ends play): the world may be gone.
	if (UWorld* World = GetWorld())
	{
		if (UHollowSectorAssemblySubsystem* Assembly = World->GetSubsystem<UHollowSectorAssemblySubsystem>())
		{
			Assembly->OnAssembled.Remove(AssembledHandle);
			Assembly->Teardown();
		}
		World->GetTimerManager().ClearTimer(RetryTimer);
	}
	ReleaseSector();
	Super::EndPlay(EndPlayReason);
}

void UHollowSectorLoaderComponent::HandleManifest(const FHollowSectorManifest& Manifest)
{
	if (Manifest.BuildId == RequestedBuildId)
	{
		return;
	}
	RequestedBuildId = Manifest.BuildId;
	PendingReport.Reset();
	GetWorld()->GetTimerManager().ClearTimer(RetryTimer);
	GetWorld()->GetSubsystem<UHollowSectorAssemblySubsystem>()->Teardown();
	if (!Manifest.IsSet())
	{
		ReleaseSector();
		return;
	}

	const FString Sector = Manifest.SectorPlugin.ToString();
	UHollowSectorFeatureSubsystem* Sectors = UHollowSectorFeatureSubsystem::Get();
	if (Sector == ActiveSector || !Sectors)
	{
		HandleSectorActive(Manifest.BuildId, Sector == ActiveSector);
		return;
	}
	ReleaseSector();
	ActiveSector = Sector;
	const int32 BuildId = Manifest.BuildId;
	Sectors->ActivateSector(Sector, UHollowSectorFeatureSubsystem::FOnHollowSectorChanged::CreateWeakLambda(this,
		[this, BuildId](const bool bActive) { HandleSectorActive(BuildId, bActive); }));
}

void UHollowSectorLoaderComponent::HandleSectorActive(const int32 ForBuildId, const bool bActive)
{
	if (ForBuildId != RequestedBuildId)
	{
		return;   // a newer manifest arrived meanwhile
	}
	// Registered content is mounted already, so assembly can still work if activation failed (logged by the subsystem).
	UE_CLOG(!bActive, LogHollowGame, Warning, TEXT("Sector %s is not active; assembling build %d from registered content."), *ActiveSector, ForBuildId);
	const UHollowSectorStateComponent* State = GetOwner()->FindComponentByClass<UHollowSectorStateComponent>();
	GetWorld()->GetSubsystem<UHollowSectorAssemblySubsystem>()->Assemble(State->GetManifest());
}

void UHollowSectorLoaderComponent::HandleAssembled(const int32 BuildId, const uint32 LocalHash)
{
	if (BuildId != RequestedBuildId)
	{
		return;
	}
	PendingReport = TPair<int32, uint32>(BuildId, LocalHash);
	TryReport();
}

void UHollowSectorLoaderComponent::TryReport()
{
	if (!PendingReport.IsSet())
	{
		return;
	}
	UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World->GetGameInstance();
	AHollowPlayerController* Player = GameInstance ? Cast<AHollowPlayerController>(GameInstance->GetFirstLocalPlayerController(World)) : nullptr;
	if (Player)
	{
		UE_LOG(LogHollowGame, Log, TEXT("Reporting sector build %d, hash %08X"), PendingReport->Key, PendingReport->Value);
		Player->Server_ReportSectorReady(PendingReport->Key, PendingReport->Value);
		PendingReport.Reset();
		return;
	}
	if (World->GetNetMode() == NM_DedicatedServer)
	{
		PendingReport.Reset();   // nobody to report for; the authority follows the server's own assembly
		return;
	}
	World->GetTimerManager().SetTimer(RetryTimer, this, &ThisClass::TryReport, GetDefault<UHollowSectorSettings>()->ReportRetrySeconds, /*bLoop=*/false);
}

void UHollowSectorLoaderComponent::ReleaseSector()
{
	if (ActiveSector.IsEmpty())
	{
		return;
	}
	if (UHollowSectorFeatureSubsystem* Sectors = UHollowSectorFeatureSubsystem::Get())
	{
		Sectors->DeactivateSector(ActiveSector, {});
	}
	ActiveSector.Empty();
}
