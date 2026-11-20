#include "Raid/HollowRaidStateComponent.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "HollowGameLog.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Profile/HollowProfileSubsystem.h"
#include "Util/HollowEnumUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowRaidStateComponent)

UHollowRaidStateComponent::UHollowRaidStateComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

UHollowRaidStateComponent* UHollowRaidStateComponent::Get(const UWorld* World)
{
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->FindComponentByClass<UHollowRaidStateComponent>() : nullptr;
}

void UHollowRaidStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UHollowRaidStateComponent, State, Params);
}

void UHollowRaidStateComponent::SetState(const FHollowRaidNetState& NewState)
{
	if (!GetOwner()->HasAuthority() || NewState == State)
	{
		return;
	}
	const FHollowRaidNetState OldState = State;
	State = NewState;
	MARK_PROPERTY_DIRTY_FROM_NAME(UHollowRaidStateComponent, State, this);
	GetOwner()->ForceNetUpdate();
	HandleChanged(OldState);
}

double UHollowRaidStateComponent::GetServerTime() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
}

float UHollowRaidStateComponent::GetSecondsLeft() const
{
	return State.PhaseEndServerTime > 0.f ? FMath::Max(0.f, static_cast<float>(State.PhaseEndServerTime - GetServerTime())) : 0.f;
}

void UHollowRaidStateComponent::OnRep_State(const FHollowRaidNetState& OldState)
{
	HandleChanged(OldState);
}

void UHollowRaidStateComponent::HandleChanged(const FHollowRaidNetState& OldState)
{
	UE_CLOG(OldState.Phase != State.Phase, LogHollowGame, Log, TEXT("Raid %s: %s -> %s (%.1f s left)"), *State.RaidId.ToString(),
		*HollowEnumToString(OldState.Phase), *HollowEnumToString(State.Phase), GetSecondsLeft());
	// The pawns are coming: this machine's profile goes into escrow for this raid (every machine, the host included).
	if (State.RaidId.IsValid() && State.Phase != EHollowRaidPhase::Assembling && State.Phase != EHollowRaidPhase::Debrief)
	{
		if (UHollowProfileSubsystem* Profile = UHollowProfileSubsystem::Get(this))
		{
			Profile->BeginRaid(State.RaidId);
		}
	}
	OnStateChanged.Broadcast(OldState, State);
}
