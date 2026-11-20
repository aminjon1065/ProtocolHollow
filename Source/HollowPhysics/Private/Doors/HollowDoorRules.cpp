#include "Doors/HollowDoorRules.h"

#include "Doors/HollowDoorDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowDoorRules)

namespace HollowDoorRulesPrivate
{
	constexpr float QuantizeScale = TNumericLimits<uint8>::Max();

	EHollowDoorOutcome ChangeState(FHollowDoorServerState& InOut, const EHollowDoorState NewState, const float ServerTime)
	{
		InOut.Net.State = NewState;
		InOut.Net.StateServerTime = ServerTime;
		++InOut.Net.TransitionSerial;   // wraps at 255: only "changed" matters
		InOut.Net.WeldQ = HollowDoorRules::Quantize(InOut.Weld);
		return EHollowDoorOutcome::StateChanged;
	}

	EHollowDoorOutcome ChangeWeld(FHollowDoorServerState& InOut, const float NewWeld)
	{
		if (NewWeld == InOut.Weld)
		{
			return EHollowDoorOutcome::Unchanged;
		}
		InOut.Weld = NewWeld;
		return EHollowDoorOutcome::WeldChanged;
	}
}

float HollowDoorRules::ApplyWeld(const float Weld, const float WorkUnits, const UHollowDoorDefinition& Definition)
{
	return FMath::Clamp(Weld + FMath::Max(WorkUnits, 0.f) * Definition.WeldPerWorkUnit, 0.f, 1.f);
}

float HollowDoorRules::ApplyCut(const float Weld, const float WorkUnits, const UHollowDoorDefinition& Definition)
{
	return FMath::Clamp(Weld - FMath::Max(WorkUnits, 0.f) * Definition.CutPerWorkUnit, 0.f, 1.f);
}

FHollowStructuralResult HollowDoorRules::ApplyStructural(const float Weld, const float Structure, const float Damage, const UHollowDoorDefinition& Definition)
{
	const float Points = FMath::Max(Damage, 0.f);
	const float Soaked = FMath::Min(Points, FMath::Max(Weld, 0.f) * Definition.WeldAbsorbFactor);
	const float Rest = Points - Soaked;
	FHollowStructuralResult Result;
	Result.Weld = Definition.WeldAbsorbFactor > 0.f ? FMath::Clamp(Weld - Soaked / Definition.WeldAbsorbFactor, 0.f, 1.f) : FMath::Clamp(Weld, 0.f, 1.f);
	Result.Structure = Definition.StructureHitPoints > 0.f ? FMath::Clamp(Structure - Rest / Definition.StructureHitPoints, 0.f, 1.f)
		: (Rest > 0.f ? 0.f : FMath::Clamp(Structure, 0.f, 1.f));
	// Repeated subtraction leaves float dust: a weld or structure that is practically gone is gone.
	Result.Weld = Result.Weld <= UE_KINDA_SMALL_NUMBER ? 0.f : Result.Weld;
	Result.Structure = Result.Structure <= UE_KINDA_SMALL_NUMBER ? 0.f : Result.Structure;
	return Result;
}

uint8 HollowDoorRules::Quantize(const float Unit)
{
	return static_cast<uint8>(FMath::RoundToInt(FMath::Clamp(Unit, 0.f, 1.f) * HollowDoorRulesPrivate::QuantizeScale));
}

float HollowDoorRules::Dequantize(const uint8 Quantized)
{
	return Quantized / HollowDoorRulesPrivate::QuantizeScale;
}

bool HollowDoorRules::ShouldSendWeld(const uint8 SentQ, const uint8 NewQ, const double SecondsSinceSend, const float Interval)
{
	return NewQ != SentQ && SecondsSinceSend >= Interval;
}

EHollowDoorOutcome HollowDoorRules::Apply(FHollowDoorServerState& InOut, const FHollowDoorEvent& Event, const UHollowDoorDefinition& Definition)
{
	using namespace HollowDoorRulesPrivate;
	const EHollowDoorState State = InOut.Net.State;
	switch (Event.Type)
	{
	case EHollowDoorEvent::Interact:
		if (State == EHollowDoorState::Closed && !InOut.Net.HasFlag(EHollowDoorFlags::Locked))
		{
			InOut.Weld = 0.f;   // opening breaks a weld that never sealed
			return ChangeState(InOut, EHollowDoorState::Opening, Event.ServerTime);
		}
		if (State == EHollowDoorState::Open && Event.bDoorwayClear)
		{
			return ChangeState(InOut, EHollowDoorState::Closing, Event.ServerTime);
		}
		return EHollowDoorOutcome::Rejected;   // welded, locked, moving, breached, or someone in the doorway

	case EHollowDoorEvent::Weld:
		if (State == EHollowDoorState::Closed)
		{
			const EHollowDoorOutcome Outcome = ChangeWeld(InOut, ApplyWeld(InOut.Weld, Event.Amount, Definition));
			return InOut.Weld >= Definition.SealThreshold ? ChangeState(InOut, EHollowDoorState::Welded, Event.ServerTime) : Outcome;
		}
		if (State == EHollowDoorState::Welded)
		{
			return ChangeWeld(InOut, ApplyWeld(InOut.Weld, Event.Amount, Definition));
		}
		return EHollowDoorOutcome::Rejected;

	case EHollowDoorEvent::Cut:
		if (State == EHollowDoorState::Welded)
		{
			const EHollowDoorOutcome Outcome = ChangeWeld(InOut, ApplyCut(InOut.Weld, Event.Amount, Definition));
			return InOut.Weld <= 0.f ? ChangeState(InOut, EHollowDoorState::Closed, Event.ServerTime) : Outcome;
		}
		if (State == EHollowDoorState::Closed && InOut.Weld > 0.f)
		{
			return ChangeWeld(InOut, ApplyCut(InOut.Weld, Event.Amount, Definition));
		}
		return EHollowDoorOutcome::Rejected;

	case EHollowDoorEvent::OpenFinished:
		return State == EHollowDoorState::Opening ? ChangeState(InOut, EHollowDoorState::Open, Event.ServerTime) : EHollowDoorOutcome::Rejected;

	case EHollowDoorEvent::CloseFinished:
		return State == EHollowDoorState::Closing ? ChangeState(InOut, EHollowDoorState::Closed, Event.ServerTime) : EHollowDoorOutcome::Rejected;

	case EHollowDoorEvent::Structural:
	{
		if (State == EHollowDoorState::Breached || Event.Amount <= 0.f)
		{
			return EHollowDoorOutcome::Rejected;
		}
		// Rows 7-8: the state stays as it is (a dented weld is still a weld) until the structure gives way.
		const FHollowStructuralResult Result = ApplyStructural(InOut.Weld, InOut.Structure, Event.Amount, Definition);
		const bool bChanged = Result.Weld != InOut.Weld || Result.Structure != InOut.Structure;
		InOut.Weld = Result.Weld;
		InOut.Structure = Result.Structure;
		InOut.Net.StructureQ = Quantize(InOut.Structure);
		if (InOut.Structure <= 0.f)
		{
			if (Event.bBackwards)
			{
				InOut.Net.Flags |= static_cast<uint8>(EHollowDoorFlags::BreachedBackwards);
			}
			return ChangeState(InOut, EHollowDoorState::Breached, Event.ServerTime);
		}
		return bChanged ? EHollowDoorOutcome::StructureChanged : EHollowDoorOutcome::Unchanged;
	}
	}
	return EHollowDoorOutcome::Rejected;
}

bool HollowDoorRules::Accepts(const FHollowDoorServerState& State, const FHollowDoorEvent& Event, const UHollowDoorDefinition& Definition)
{
	FHollowDoorServerState Copy = State;
	return Apply(Copy, Event, Definition) != EHollowDoorOutcome::Rejected;
}

float HollowDoorRules::LeafAlpha(const FHollowDoorNetState& State, const double ServerNow, const UHollowDoorDefinition& Definition)
{
	const float Progress = FMath::Clamp(static_cast<float>((ServerNow - State.StateServerTime) / Definition.OpenDuration), 0.f, 1.f);
	const float Eased = FMath::InterpEaseInOut(0.f, 1.f, Progress, Definition.OpenEaseExponent);
	switch (State.State)
	{
	case EHollowDoorState::Open:
	case EHollowDoorState::Breached: return 1.f;
	case EHollowDoorState::Opening:  return Eased;
	case EHollowDoorState::Closing:  return 1.f - Eased;
	default:                         return 0.f;
	}
}

bool HollowDoorRules::IsLeafBlocking(const EHollowDoorState State, const float Alpha, const UHollowDoorDefinition& Definition)
{
	switch (State)
	{
	case EHollowDoorState::Open:
	case EHollowDoorState::Breached: return false;
	case EHollowDoorState::Opening:  return Alpha < Definition.PassableFraction;
	default:                         return true;   // Closed, Welded, and Closing (it only starts on a free doorway)
	}
}

bool HollowDoorRules::IsFreshTransition(const FHollowDoorNetState& Old, const FHollowDoorNetState& New, const double ServerNow, const UHollowDoorDefinition& Definition)
{
	return New.TransitionSerial != Old.TransitionSerial && ServerNow - New.StateServerTime < Definition.TransitionReplayWindow;
}
