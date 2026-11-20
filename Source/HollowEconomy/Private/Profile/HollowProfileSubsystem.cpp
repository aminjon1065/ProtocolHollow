#include "Profile/HollowProfileSubsystem.h"

#include "Engine/Engine.h"
#include "Misc/CoreMisc.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HollowEconomyLog.h"
#include "Items/HollowItemTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Profile/HollowEconomySettings.h"
#include "Profile/HollowProfileSaveGame.h"
#include "Util/HollowEnumUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowProfileSubsystem)

UHollowProfileSubsystem* UHollowProfileSubsystem::Get(const UObject* WorldContext)
{
	const UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContext);
	return GameInstance ? GameInstance->GetSubsystem<UHollowProfileSubsystem>() : nullptr;
}

bool UHollowProfileSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return Super::ShouldCreateSubsystem(Outer) && !GIsAutomationTesting;
}

void UHollowProfileSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Load();
	// A raid left without a result (crash, alt-F4) counts as abandoned: death rules (ADR-0005).
	FHollowProfileState& State = Save->State;
	HandleReport(HollowProfileRules::ResolveEscrow(State, EHollowRaidOutcome::Abandoned, FDateTime::UtcNow(),
		FHollowItemCatalog::Load(HollowProfileRules::ItemsToCatalog(State))), TEXT("unresolved raid found at startup"));
	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::HandleNetworkFailure);
	}
}

void UHollowProfileSubsystem::Deinitialize()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
	}
	Super::Deinitialize();
}

const FHollowProfile& UHollowProfileSubsystem::GetProfile() const
{
	return Save->State.Profile;
}

const FHollowRaidEscrow& UHollowProfileSubsystem::GetEscrow() const
{
	return Save->State.Escrow;
}

const FHollowRaidReport* UHollowProfileSubsystem::GetPendingReport() const
{
	return Save->State.bReportPending ? &Save->State.PendingReport : nullptr;
}

void UHollowProfileSubsystem::BeginRaid(const FGuid& RaidId)
{
	FHollowProfileState& State = Save->State;
	if (!RaidId.IsValid() || State.Escrow.RaidId == RaidId)
	{
		return;
	}
	const TOptional<FHollowRaidReport> Previous = HollowProfileRules::BeginRaid(State, RaidId, FDateTime::UtcNow(),
		FHollowItemCatalog::Load(HollowProfileRules::ItemsToCatalog(State)));
	UE_LOG(LogHollowEconomy, Log, TEXT("Profile: raid %s in escrow (%d loadout slots)"), *RaidId.ToString(), State.Escrow.Loadout.Num());
	HandleReport(Previous, TEXT("another raid started before the last one was resolved"));
	Persist();
}

void UHollowProfileSubsystem::ApplyRaidResult(const FHollowRaidResult& Result)
{
	FHollowProfileState& State = Save->State;
	const TOptional<FHollowRaidReport> Report = HollowProfileRules::ApplyResult(State, Result,
		FHollowItemCatalog::Load(HollowProfileRules::ItemsToCatalog(State, &Result)));
	UE_CLOG(!Report, LogHollowEconomy, Warning, TEXT("Profile: result for raid %s ignored (escrow %s)"), *Result.RaidId.ToString(),
		*State.Escrow.RaidId.ToString());
	HandleReport(Report, TEXT("host's result"));
}

void UHollowProfileSubsystem::AcknowledgeReport()
{
	if (Save->State.bReportPending)
	{
		Save->State.bReportPending = false;
		Persist();
	}
}

bool UHollowProfileSubsystem::SetSlotInsured(const int32 SlotIndex, const bool bInsured)
{
	TArray<FHollowLoadoutSlot>& Loadout = Save->State.Profile.Loadout;
	if (!Loadout.IsValidIndex(SlotIndex))
	{
		return false;
	}
	FHollowLoadoutSlot& Slot = Loadout[SlotIndex];
	if (bInsured && !FHollowItemCatalog::Load(MakeArrayView(&Slot.Item, 1)).IsInsurable(Slot.Item))
	{
		return false;
	}
	Slot.bInsured = bInsured;
	Persist();
	return true;
}

void UHollowProfileSubsystem::ResetProfile()
{
	MakeNewProfile();
	Persist();
}

void UHollowProfileSubsystem::Load()
{
	const UHollowEconomySettings* Settings = GetDefault<UHollowEconomySettings>();
	Save = UGameplayStatics::DoesSaveGameExist(Settings->SaveSlotName, Settings->SaveUserIndex)
		? Cast<UHollowProfileSaveGame>(UGameplayStatics::LoadGameFromSlot(Settings->SaveSlotName, Settings->SaveUserIndex)) : nullptr;
	if (Save && Save->Version == UHollowProfileSaveGame::CurrentVersion)
	{
		UE_LOG(LogHollowEconomy, Log, TEXT("Profile loaded from %s: %d credits, %d loadout slots"), *Settings->SaveSlotName,
			Save->State.Profile.Credits, Save->State.Profile.Loadout.Num());
		return;
	}
	UE_CLOG(Save, LogHollowEconomy, Warning, TEXT("Profile %s has version %d (expected %d): starting a new one"), *Settings->SaveSlotName,
		Save->Version, UHollowProfileSaveGame::CurrentVersion);
	Save = Cast<UHollowProfileSaveGame>(UGameplayStatics::CreateSaveGameObject(UHollowProfileSaveGame::StaticClass()));
	MakeNewProfile();
	Persist();
}

void UHollowProfileSubsystem::MakeNewProfile()
{
	const UHollowEconomySettings* Settings = GetDefault<UHollowEconomySettings>();
	Save->State = FHollowProfileState();
	Save->State.Profile.Credits = Settings->StartingCredits;
	Save->State.Profile.Loadout = Settings->StartingLoadout;
	UE_LOG(LogHollowEconomy, Log, TEXT("Profile: new profile with %d credits and %d loadout slots"), Settings->StartingCredits, Settings->StartingLoadout.Num());
}

void UHollowProfileSubsystem::Persist() const
{
	const UHollowEconomySettings* Settings = GetDefault<UHollowEconomySettings>();
	UE_CLOG(!UGameplayStatics::SaveGameToSlot(Save, Settings->SaveSlotName, Settings->SaveUserIndex), LogHollowEconomy, Error,
		TEXT("Profile could not be saved to %s"), *Settings->SaveSlotName);
}

void UHollowProfileSubsystem::HandleReport(const TOptional<FHollowRaidReport>& Report, const TCHAR* Why)
{
	if (!Report)
	{
		return;
	}
	UE_LOG(LogHollowEconomy, Log, TEXT("Profile: raid %s resolved as %s (%s): credits %d -> %d, %d item(s) kept, %d lost, %d sold"),
		*Report->Result.RaidId.ToString(), *HollowEnumToString(Report->Result.Outcome), Why, Report->CreditsBefore, Report->CreditsAfter,
		Report->Delta.Kept.Num(), Report->Delta.Lost.Num(), Report->Delta.Sold.Num());
	Persist();
	OnRaidReport.Broadcast(Report.GetValue());
}

void UHollowProfileSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, const ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	// Only a client loses its host; a host's own failures never end the raid it runs.
	if (!World || World->GetGameInstance() != GetGameInstance() || World->GetNetMode() != NM_Client || !Save->State.Escrow.IsActive())
	{
		return;
	}
	FHollowProfileState& State = Save->State;
	HandleReport(HollowProfileRules::ResolveEscrow(State, EHollowRaidOutcome::SignalLost, FDateTime::UtcNow(),
		FHollowItemCatalog::Load(HollowProfileRules::ItemsToCatalog(State))), *FString::Printf(TEXT("connection to the host lost: %s"),
		ENetworkFailure::ToString(FailureType)));
}
