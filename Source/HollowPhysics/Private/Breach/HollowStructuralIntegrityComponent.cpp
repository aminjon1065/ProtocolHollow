#include "Breach/HollowStructuralIntegrityComponent.h"

#include "Breach/HollowBreachableDefinition.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "HollowPhysicsLog.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Util/HollowEnumUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowStructuralIntegrityComponent)

UHollowStructuralIntegrityComponent::UHollowStructuralIntegrityComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UHollowStructuralIntegrityComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner()->HasAuthority() && Definition)
	{
		Health = HollowBreachRules::MakeInitial(*Definition).Health;
	}
	UE_CLOG(!Definition, LogHollowPhysics, Error, TEXT("%s: breachable object has no definition."), *GetNameSafe(GetOwner()));
}

void UHollowStructuralIntegrityComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UHollowStructuralIntegrityComponent, NetState, Params);
}

double UHollowStructuralIntegrityComponent::GetServerTime() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
}

bool UHollowStructuralIntegrityComponent::CanTakeDamage() const
{
	return Definition && NetState.State != EHollowBreachState::Breached;
}

float UHollowStructuralIntegrityComponent::GetZoneHealth(const int32 Zone) const
{
	return Health.IsValidIndex(Zone) ? Health[Zone] : 0.f;
}

EHollowBreachOutcome UHollowStructuralIntegrityComponent::ApplyHit(const FVector& WorldLocation, const float Amount, const FVector& WorldDirection)
{
	if (!GetOwner()->HasAuthority() || !Definition)
	{
		return EHollowBreachOutcome::Rejected;
	}
	FHollowBreachServerState State;
	State.Net = NetState;
	State.Health = Health;
	FHollowBreachHit Hit;
	Hit.LocalPoint = GetOwner()->GetActorTransform().InverseTransformPosition(WorldLocation);
	Hit.Amount = Amount;
	Hit.Direction = WorldDirection;
	Hit.ServerTime = static_cast<float>(GetServerTime());
	const EHollowBreachOutcome Outcome = HollowBreachRules::ApplyHit(State, Hit, *Definition);
	if (Outcome == EHollowBreachOutcome::Rejected)
	{
		return Outcome;
	}

	const FHollowBreachNetState Old = NetState;
	NetState = State.Net;
	Health = State.Health;
	UE_CLOG(Outcome == EHollowBreachOutcome::ZoneBroken, LogHollowPhysics, Log, TEXT("%s: zones %02X -> %02X, %s"), *GetNameSafe(GetOwner()),
		Old.ZoneMask, NetState.ZoneMask, *HollowEnumToString(NetState.State));
	if (NetState == Old)
	{
		return Outcome;   // a blow too small to move the damage stage: nothing to send
	}
	// Dormant between changes (ADR-0002): wake, mark, send now.
	GetOwner()->FlushNetDormancy();
	MARK_PROPERTY_DIRTY_FROM_NAME(UHollowStructuralIntegrityComponent, NetState, this);
	GetOwner()->ForceNetUpdate();
	OnStateChanged.Broadcast(Old, NetState);
	return Outcome;
}

void UHollowStructuralIntegrityComponent::OnRep_NetState(const FHollowBreachNetState& OldState)
{
	UE_CLOG(OldState.ZoneMask != NetState.ZoneMask, LogHollowPhysics, Log, TEXT("%s: zones %02X -> %02X, %s (replicated)"), *GetNameSafe(GetOwner()),
		OldState.ZoneMask, NetState.ZoneMask, *HollowEnumToString(NetState.State));
	OnStateChanged.Broadcast(OldState, NetState);
}
