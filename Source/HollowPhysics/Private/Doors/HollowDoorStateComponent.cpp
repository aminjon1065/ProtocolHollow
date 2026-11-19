#include "Doors/HollowDoorStateComponent.h"

#include "Doors/HollowDoorDefinition.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "HollowPhysicsLog.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Util/HollowEnumUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowDoorStateComponent)

UHollowDoorStateComponent::UHollowDoorStateComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UHollowDoorStateComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner()->HasAuthority() && Definition)
	{
		NetState.State = Definition->InitialState;
		NetState.Flags = static_cast<uint8>(EHollowDoorFlags::Powered);
		NetState.StateServerTime = static_cast<float>(GetServerTime());
		MARK_PROPERTY_DIRTY_FROM_NAME(UHollowDoorStateComponent, NetState, this);
	}
	UE_CLOG(!Definition, LogHollowPhysics, Error, TEXT("%s: door has no definition."), *GetNameSafe(GetOwner()));
}

void UHollowDoorStateComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(TransitionTimer);
	GetWorld()->GetTimerManager().ClearTimer(WeldFlushTimer);
	Super::EndPlay(EndPlayReason);
}

void UHollowDoorStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UHollowDoorStateComponent, NetState, Params);
}

float UHollowDoorStateComponent::GetWeld() const
{
	return GetOwner() && GetOwner()->HasAuthority() ? Weld : HollowDoorRules::Dequantize(NetState.WeldQ);
}

double UHollowDoorStateComponent::GetServerTime() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
}

TOptional<EHollowDoorEvent> UHollowDoorStateComponent::ToolEvent(const FGameplayTag& ToolTag) const
{
	if (Definition && ToolTag.IsValid())
	{
		if (ToolTag == Definition->WeldToolTag)
		{
			return EHollowDoorEvent::Weld;
		}
		if (ToolTag == Definition->CutToolTag)
		{
			return EHollowDoorEvent::Cut;
		}
	}
	return {};
}

bool UHollowDoorStateComponent::Accepts(const EHollowDoorEvent Type, const bool bDoorwayClear) const
{
	if (!Definition)
	{
		return false;
	}
	FHollowDoorServerState State;
	State.Net = NetState;
	State.Weld = GetWeld();
	FHollowDoorEvent Event;
	Event.Type = Type;
	Event.Amount = UE_KINDA_SMALL_NUMBER;   // any work: is this kind of event allowed at all
	Event.bDoorwayClear = bDoorwayClear;
	return HollowDoorRules::Accepts(State, Event, *Definition);
}

EHollowDoorOutcome UHollowDoorStateComponent::Interact(const bool bDoorwayClear)
{
	FHollowDoorEvent Event;
	Event.Type = EHollowDoorEvent::Interact;
	Event.bDoorwayClear = bDoorwayClear;
	return Handle(Event);
}

EHollowDoorOutcome UHollowDoorStateComponent::ApplyTool(const EHollowDoorEvent Type, const float WorkUnits)
{
	FHollowDoorEvent Event;
	Event.Type = Type;
	Event.Amount = WorkUnits;
	return Handle(Event);
}

EHollowDoorOutcome UHollowDoorStateComponent::Handle(const FHollowDoorEvent& InEvent)
{
	if (!GetOwner()->HasAuthority() || !Definition)
	{
		return EHollowDoorOutcome::Rejected;
	}

	FHollowDoorEvent Event = InEvent;
	Event.ServerTime = static_cast<float>(GetServerTime());
	FHollowDoorServerState State;
	State.Net = NetState;
	State.Weld = Weld;
	const FHollowDoorNetState Old = NetState;
	const EHollowDoorOutcome Outcome = HollowDoorRules::Apply(State, Event, *Definition);
	Weld = State.Weld;

	switch (Outcome)
	{
	case EHollowDoorOutcome::StateChanged:
		NetState = State.Net;
		LastWeldSendTime = GetServerTime();
		GetWorld()->GetTimerManager().ClearTimer(WeldFlushTimer);
		UE_LOG(LogHollowPhysics, Log, TEXT("%s: %s -> %s (weld %.2f)"), *GetNameSafe(GetOwner()), *HollowEnumToString(Old.State),
			*HollowEnumToString(NetState.State), Weld);
		if (NetState.State == EHollowDoorState::Opening || NetState.State == EHollowDoorState::Closing)
		{
			GetWorld()->GetTimerManager().SetTimer(TransitionTimer, this, &ThisClass::FinishTransition, Definition->OpenDuration, /*bLoop=*/false);
		}
		Publish(Old, /*bFresh=*/true);
		break;
	case EHollowDoorOutcome::WeldChanged:
		SendWeldIfDue();
		break;
	default:
		break;
	}
	return Outcome;
}

void UHollowDoorStateComponent::SendWeldIfDue()
{
	if (HollowDoorRules::ShouldSendWeld(NetState.WeldQ, HollowDoorRules::Quantize(Weld), GetServerTime() - LastWeldSendTime, Definition->WeldSendInterval))
	{
		SendWeld();
	}
	else if (HollowDoorRules::Quantize(Weld) != NetState.WeldQ && !GetWorld()->GetTimerManager().IsTimerActive(WeldFlushTimer))
	{
		// The last change of a weld session goes out one interval later, whether or not the tool still runs.
		GetWorld()->GetTimerManager().SetTimer(WeldFlushTimer, this, &ThisClass::FlushWeld, Definition->WeldSendInterval, /*bLoop=*/false);
	}
}

void UHollowDoorStateComponent::FlushWeld()
{
	if (HollowDoorRules::Quantize(Weld) != NetState.WeldQ)
	{
		SendWeld();
	}
}

void UHollowDoorStateComponent::SendWeld()
{
	const FHollowDoorNetState Old = NetState;
	NetState.WeldQ = HollowDoorRules::Quantize(Weld);
	LastWeldSendTime = GetServerTime();
	GetWorld()->GetTimerManager().ClearTimer(WeldFlushTimer);
	Publish(Old, /*bFresh=*/false);
}

void UHollowDoorStateComponent::FinishTransition()
{
	FHollowDoorEvent Event;
	Event.Type = NetState.State == EHollowDoorState::Opening ? EHollowDoorEvent::OpenFinished : EHollowDoorEvent::CloseFinished;
	Handle(Event);
}

void UHollowDoorStateComponent::Publish(const FHollowDoorNetState& Old, const bool bFresh)
{
	// Dormant between changes (ADR-0002): wake, mark, send now.
	UE_LOG(LogHollowPhysics, Verbose, TEXT("%s: %s weld %d sent"), *GetNameSafe(GetOwner()), *HollowEnumToString(NetState.State), NetState.WeldQ);
	GetOwner()->FlushNetDormancy();
	MARK_PROPERTY_DIRTY_FROM_NAME(UHollowDoorStateComponent, NetState, this);
	GetOwner()->ForceNetUpdate();
	OnStateChanged.Broadcast(Old, NetState, bFresh);
}

void UHollowDoorStateComponent::OnRep_NetState(const FHollowDoorNetState& OldState)
{
	const bool bFresh = Definition && HollowDoorRules::IsFreshTransition(OldState, NetState, GetServerTime(), *Definition);
	UE_CLOG(OldState.State != NetState.State, LogHollowPhysics, Log, TEXT("%s: %s -> %s (replicated, %s)"), *GetNameSafe(GetOwner()),
		*HollowEnumToString(OldState.State), *HollowEnumToString(NetState.State), bFresh ? TEXT("fresh") : TEXT("snap"));
	UE_CLOG(OldState.WeldQ != NetState.WeldQ, LogHollowPhysics, Verbose, TEXT("%s: weld %d received"), *GetNameSafe(GetOwner()), NetState.WeldQ);
	OnStateChanged.Broadcast(OldState, NetState, bFresh);
}
