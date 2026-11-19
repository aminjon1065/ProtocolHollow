#include "Assembly/HollowSectorStateComponent.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "HollowPCGLog.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowSectorStateComponent)

UHollowSectorStateComponent::UHollowSectorStateComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

UHollowSectorStateComponent* UHollowSectorStateComponent::Get(const UWorld* World)
{
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->FindComponentByClass<UHollowSectorStateComponent>() : nullptr;
}

void UHollowSectorStateComponent::SetManifest(const FHollowSectorManifest& NewManifest)
{
	if (!ensureMsgf(GetOwner() && GetOwner()->HasAuthority(), TEXT("The sector manifest is server-owned")))
	{
		return;
	}
	Manifest = NewManifest;
	MARK_PROPERTY_DIRTY_FROM_NAME(UHollowSectorStateComponent, Manifest, this);
	GetOwner()->ForceNetUpdate();
	UE_LOG(LogHollowPCG, Log, TEXT("Sector manifest: build %d, seed %d, %d rooms, %d connections, hash %08X, ~%d bytes"), Manifest.BuildId,
		Manifest.Seed, Manifest.Rooms.Num(), Manifest.Connections.Num(), Manifest.LayoutHash, HollowSectorManifest::EstimateNetBytes(Manifest));
	OnManifestChanged.Broadcast(Manifest);
}

void UHollowSectorStateComponent::SetSectorReady(const bool bReady)
{
	if (!ensureMsgf(GetOwner() && GetOwner()->HasAuthority(), TEXT("Sector readiness is server-owned")) || bSectorReady == bReady)
	{
		return;
	}
	bSectorReady = bReady;
	MARK_PROPERTY_DIRTY_FROM_NAME(UHollowSectorStateComponent, bSectorReady, this);
	GetOwner()->ForceNetUpdate();
	OnSectorReadyChanged.Broadcast(bSectorReady);
}

void UHollowSectorStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Push model, not COND_InitialOnly: the manifest arrives after the GameState's first replication (ADR-0003).
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UHollowSectorStateComponent, Manifest, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UHollowSectorStateComponent, bSectorReady, Params);
}

void UHollowSectorStateComponent::OnRep_Manifest()
{
	UE_LOG(LogHollowPCG, Log, TEXT("Sector manifest received: build %d, seed %d, %d rooms, hash %08X"), Manifest.BuildId, Manifest.Seed,
		Manifest.Rooms.Num(), Manifest.LayoutHash);
	OnManifestChanged.Broadcast(Manifest);
}

void UHollowSectorStateComponent::OnRep_SectorReady()
{
	OnSectorReadyChanged.Broadcast(bSectorReady);
}
