#include "Radio/HollowRadioRules.h"

#include "Misc/Crc.h"
#include "Tags/HollowGameplayTags.h"

namespace HollowRadioRulesPrivate
{
	/** Enough for any real chain of lines and cues in one call; guards a script with zero-length lines. */
	constexpr int32 MaxSteps = 256;
}

void FHollowRadioQueue::SetScript(const UHollowRadioScript* InScript)
{
	if (Script.Get() != InScript)
	{
		Script = InScript;
		Clear();
		Forget();
	}
}

const FHollowRadioCue* FHollowRadioQueue::CueAt(const int32 Index) const
{
	const UHollowRadioScript* Lines = Script.Get();
	return Lines && Lines->Cues.IsValidIndex(Index) ? &Lines->Cues[Index] : nullptr;
}

EHollowRadioPriority FHollowRadioQueue::PriorityOf(const int32 Cue) const
{
	const FHollowRadioCue* Found = CueAt(Cue);
	return Found ? Found->Priority : EHollowRadioPriority::Low;
}

const FHollowRadioCue* FHollowRadioQueue::GetPlayingCue() const
{
	return Playing.IsSet() ? CueAt(Playing->Cue) : nullptr;
}

const FHollowRadioLine* FHollowRadioQueue::GetPlayingLine() const
{
	const FHollowRadioCue* Cue = GetPlayingCue();
	if (!Cue || !Cue->Variants.IsValidIndex(Playing->Variant))
	{
		return nullptr;
	}
	const TArray<FHollowRadioLine>& Lines = Cue->Variants[Playing->Variant].Lines;
	return Lines.IsValidIndex(Playing->Line) ? &Lines[Playing->Line] : nullptr;
}

bool FHollowRadioQueue::IsQueued(const int32 Cue) const
{
	return Queue.ContainsByPredicate([Cue](const FRequest& Request) { return Request.Cue == Cue; });
}

void FHollowRadioQueue::Clear()
{
	Queue.Reset();
	Playing.Reset();
	PendingDropped.Reset();
	bCut = false;
	LastEnded.Reset();
}

void FHollowRadioQueue::Forget()
{
	Fired.Reset();
	LastStarted.Reset();
	Plays.Reset();
}

int32 FHollowRadioQueue::PickVariant(const int32 Cue, const FGuid& RaidId) const
{
	const FHollowRadioCue* Found = CueAt(Cue);
	if (!Found || Found->Variants.IsEmpty())
	{
		return 0;
	}
	// The raid picks it (the same on every machine), a Repeat cue's plays take turns; never random.
	const uint32 Base = HashCombine(GetTypeHash(RaidId), FCrc::StrCrc32(*Found->Id.ToString()));
	const int32* Played = Plays.Find(Cue);
	const uint32 Turn = Found->Scope == EHollowRadioScope::Repeat && Played ? static_cast<uint32>(*Played) : 0u;
	return static_cast<int32>((Base + Turn) % static_cast<uint32>(Found->Variants.Num()));
}

int32 FHollowRadioQueue::Fire(const FGameplayTag& Trigger, const FName Sector, const FGuid& RaidId, const double Now,
	TFunctionRef<bool(FName)> HeardForGood, const bool bForce)
{
	const UHollowRadioScript* Lines = Script.Get();
	if (!Lines)
	{
		return INDEX_NONE;
	}
	if (RaidId != Raid)
	{
		Raid = RaidId;   // a raid's memory is its own
		Forget();
	}
	const int32 CueIndex = Lines->FindCue(Trigger, Sector);
	const FHollowRadioCue* Cue = CueAt(CueIndex);
	if (!Cue || Cue->Variants.IsEmpty() || (Playing.IsSet() && Playing->Cue == CueIndex) || IsQueued(CueIndex))
	{
		return INDEX_NONE;   // one of each at a time, even forced
	}
	if (!bForce)
	{
		switch (Cue->Scope)
		{
		case EHollowRadioScope::Profile:
			if (Fired.Contains(CueIndex) || HeardForGood(Cue->Id))
			{
				return INDEX_NONE;
			}
			break;
		case EHollowRadioScope::Raid:
			if (Fired.Contains(CueIndex))
			{
				return INDEX_NONE;
			}
			break;
		case EHollowRadioScope::Repeat:
			if (const double* Last = LastStarted.Find(CueIndex); Last && Now < *Last + Cue->CooldownSeconds)
			{
				return INDEX_NONE;
			}
			break;
		}
	}
	if (Queue.Num() >= FMath::Max(Lines->MaxQueued, 1))
	{
		// Full: the least urgent of those waiting, the latest of them, makes room for a more urgent one only.
		int32 Worst = 0;
		for (int32 Index = 1; Index < Queue.Num(); ++Index)
		{
			const EHollowRadioPriority Priority = PriorityOf(Queue[Index].Cue);
			const EHollowRadioPriority WorstPriority = PriorityOf(Queue[Worst].Cue);
			if (Priority < WorstPriority || (Priority == WorstPriority && Queue[Index].Order > Queue[Worst].Order))
			{
				Worst = Index;
			}
		}
		if (PriorityOf(Queue[Worst].Cue) >= Cue->Priority)
		{
			return INDEX_NONE;
		}
		PendingDropped.Add(Queue[Worst].Cue);
		Queue.RemoveAt(Worst);
	}
	if (Cue->Scope != EHollowRadioScope::Repeat)
	{
		Fired.Add(CueIndex);
	}
	if (Cue->Priority == EHollowRadioPriority::Critical && Playing.IsSet() && PriorityOf(Playing->Cue) < EHollowRadioPriority::Critical)
	{
		// Cut in: the cut cue waits first among its equals, to go on from the line it was on.
		FRequest Resume;
		Resume.Cue = Playing->Cue;
		Resume.Variant = Playing->Variant;
		Resume.Line = Playing->Line;
		Resume.QueuedAt = Now;
		Resume.Order = -NextOrder;
		NextOrder += 1.0;
		Queue.Add(Resume);
		Playing.Reset();
		bCut = true;
		LastEnded = Now;
	}
	FRequest Request;
	Request.Cue = CueIndex;
	Request.Variant = PickVariant(CueIndex, RaidId);
	Request.QueuedAt = Now;
	Request.Order = NextOrder;
	NextOrder += 1.0;
	Queue.Add(Request);
	return CueIndex;
}

int32 FHollowRadioQueue::PickNext() const
{
	int32 Best = INDEX_NONE;
	for (int32 Index = 0; Index < Queue.Num(); ++Index)
	{
		if (Best == INDEX_NONE)
		{
			Best = Index;
			continue;
		}
		const EHollowRadioPriority Priority = PriorityOf(Queue[Index].Cue);
		const EHollowRadioPriority BestPriority = PriorityOf(Queue[Best].Cue);
		if (Priority > BestPriority || (Priority == BestPriority && Queue[Index].Order < Queue[Best].Order))
		{
			Best = Index;
		}
	}
	return Best;
}

TOptional<double> FHollowRadioQueue::GetNextWake() const
{
	if (Playing.IsSet())
	{
		return Playing->LineEnds;
	}
	const int32 Next = PickNext();
	if (Next == INDEX_NONE)
	{
		return {};
	}
	const UHollowRadioScript* Lines = Script.Get();
	const FRequest& Request = Queue[Next];
	double At = Request.QueuedAt;
	if (LastEnded.IsSet())
	{
		const double Gap = PriorityOf(Request.Cue) == EHollowRadioPriority::Critical || !Lines ? 0.0 : Lines->CueGapSeconds;
		At = FMath::Max(At, LastEnded.GetValue() + Gap);
	}
	return At;
}

void FHollowRadioQueue::Start(const FRequest& Request, const double At, FHollowRadioEvents& Events)
{
	FHollowRadioPlaying& Now = Playing.Emplace();
	Now.Cue = Request.Cue;
	Now.Variant = Request.Variant;
	Now.Line = Request.Line;
	const FHollowRadioLine* Line = GetPlayingLine();
	const UHollowRadioScript* Lines = Script.Get();
	Now.LineEnds = At + (Line && Lines ? Lines->GetLineSeconds(Line->Text) : 0.0);
	LastStarted.Add(Request.Cue, At);
	++Plays.FindOrAdd(Request.Cue);
	Events.bOpened = true;
	Events.bLineChanged = true;
}

FHollowRadioEvents FHollowRadioQueue::Advance(const double Now)
{
	FHollowRadioEvents Events;
	Events.Dropped = MoveTemp(PendingDropped);
	PendingDropped.Reset();
	if (bCut)
	{
		Events.bClosed = true;
		Events.bLineChanged = true;
		bCut = false;
	}
	const UHollowRadioScript* Lines = Script.Get();
	if (!Lines)
	{
		Clear();
		return Events;
	}
	for (int32 Step = 0; Step < HollowRadioRulesPrivate::MaxSteps; ++Step)
	{
		if (Playing.IsSet())
		{
			if (Now < Playing->LineEnds)
			{
				break;
			}
			const double Ended = Playing->LineEnds;
			const FHollowRadioCue* Cue = GetPlayingCue();
			const int32 Count = Cue && Cue->Variants.IsValidIndex(Playing->Variant) ? Cue->Variants[Playing->Variant].Lines.Num() : 0;
			Events.bLineChanged = true;
			if (Playing->Line + 1 < Count)
			{
				++Playing->Line;
				const FHollowRadioLine* Line = GetPlayingLine();
				Playing->LineEnds = Ended + (Line ? Lines->GetLineSeconds(Line->Text) : 0.0);
				continue;
			}
			Events.Finished.Add(Playing->Cue);
			Events.bClosed = true;
			Playing.Reset();
			LastEnded = Ended;
			continue;
		}
		for (int32 Index = Queue.Num() - 1; Index >= 0; --Index)
		{
			const FHollowRadioCue* Cue = CueAt(Queue[Index].Cue);
			if (!Cue || (Cue->StaleSeconds > 0.f && Now > Queue[Index].QueuedAt + Cue->StaleSeconds))
			{
				Events.Dropped.Add(Queue[Index].Cue);
				Queue.RemoveAt(Index);
			}
		}
		const TOptional<double> At = GetNextWake();
		const int32 Next = PickNext();
		if (Next == INDEX_NONE || !At.IsSet() || At.GetValue() > Now)
		{
			break;
		}
		const FRequest Request = Queue[Next];
		Queue.RemoveAt(Next);
		Start(Request, At.GetValue(), Events);
	}
	return Events;
}

namespace HollowRadioRules
{
	bool IsSortie(const FHollowRadioFacts& Facts)
	{
		return Facts.bInRaid && !Facts.Sector.IsNone() && (Facts.Phase == EHollowRaidPhase::Deploying || Facts.Phase == EHollowRaidPhase::InRaid);
	}

	bool ShouldSense(const FHollowRadioFacts& Facts)
	{
		const bool bOnTheGround = Facts.Phase == EHollowRaidPhase::InRaid || Facts.Phase == EHollowRaidPhase::Extraction;
		const bool bCorePending = !Facts.bCoreSighted && !Facts.bCoreLifted;
		return Facts.bInRaid && bOnTheGround && Facts.bAlive && (bCorePending || !Facts.bStalkerSighted);
	}

	bool IsInSight(const FVector& ViewLocation, const FVector& ViewDirection, const FVector& Target, const float MaxDistance, const float HalfAngleDegrees)
	{
		const FVector ToTarget = Target - ViewLocation;
		const double Distance = ToTarget.Size();
		if (Distance > MaxDistance)
		{
			return false;
		}
		if (Distance <= UE_KINDA_SMALL_NUMBER)
		{
			return true;
		}
		const double Cosine = FVector::DotProduct(ViewDirection.GetSafeNormal(), ToTarget / Distance);
		return Cosine >= FMath::Cos(FMath::DegreesToRadians(static_cast<double>(HalfAngleDegrees)));
	}

	TArray<FGameplayTag> TriggersFor(const FHollowRadioFacts& Old, const FHollowRadioFacts& New)
	{
		// A new raid (or leaving one): nothing of the last one counts.
		const FHollowRadioFacts Was = Old.RaidId == New.RaidId ? Old : FHollowRadioFacts();
		TArray<FGameplayTag> Triggers;
		if (New.bInHub && New.bHasBody && !(Was.bInHub && Was.bHasBody))
		{
			Triggers.Add(HollowTags::Radio_Hub_Arrival);
		}
		if (!New.bInRaid)
		{
			return Triggers;
		}
		if (IsSortie(New) && !IsSortie(Was))
		{
			Triggers.Add(HollowTags::Radio_Raid_Sortie);
		}
		if (New.bCoreSighted && !Was.bCoreSighted && !New.bCoreLifted)
		{
			Triggers.Add(HollowTags::Radio_Core_Sighted);   // lifted at once: the lift says it all
		}
		if (New.bCoreLifted && !Was.bCoreLifted)
		{
			Triggers.Add(HollowTags::Radio_Core_Lifted);
		}
		if (New.bRitual && !Was.bRitual)
		{
			Triggers.Add(HollowTags::Radio_Altar_Started);
		}
		if (New.bStalkerSighted && !Was.bStalkerSighted)
		{
			Triggers.Add(HollowTags::Radio_Stalker_Contact);
		}
		if (New.bAlive && New.Tier == EHollowResonanceTier::Critical && Was.Tier != EHollowResonanceTier::Critical)
		{
			Triggers.Add(HollowTags::Radio_Resonance_Critical);
		}
		if (New.Phase == EHollowRaidPhase::Extraction && Was.Phase != EHollowRaidPhase::Extraction)
		{
			Triggers.Add(HollowTags::Radio_Extraction_Started);
		}
		if (New.NewLore > Was.NewLore)
		{
			Triggers.Add(HollowTags::Radio_Lore_Found);
		}
		const bool bOnTheGround = New.Phase == EHollowRaidPhase::InRaid || New.Phase == EHollowRaidPhase::Extraction;
		if (Was.bInRaid && Was.bAlive && New.bAlive && bOnTheGround && New.LivingPlayers < Was.LivingPlayers)
		{
			Triggers.Add(HollowTags::Radio_Squad_Loss);   // a squad mate: this player still lives
		}
		return Triggers;
	}
}
