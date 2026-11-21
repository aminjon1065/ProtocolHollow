#include "Profile/HollowProfileTypes.h"

#include "Extraction/HollowExtractionResolver.h"
#include "Items/HollowItemTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowProfileTypes)

namespace HollowProfileRules
{
	void Apply(FHollowProfile& Profile, const FHollowProfileDelta& Delta, const EHollowRaidOutcome Outcome)
	{
		Profile.Credits = FMath::Max(0, Profile.Credits + Delta.Credits);
		for (const TArray<FHollowItemStack>* Gone : {&Delta.Spent, &Delta.Lost})
		{
			for (const FHollowItemStack& Stack : *Gone)
			{
				int32 Remaining = Stack.Count;
				for (FHollowLoadoutSlot& Slot : Profile.Loadout)
				{
					if (Remaining > 0 && Slot.Item == Stack.Item)
					{
						const int32 Taken = FMath::Min(Slot.Count, Remaining);
						Slot.Count -= Taken;
						Remaining -= Taken;
					}
				}
			}
		}
		Profile.Loadout.RemoveAll([](const FHollowLoadoutSlot& Slot) { return Slot.Count <= 0; });

		++Profile.Stats.Raids;
		Profile.Stats.Extractions += Outcome == EHollowRaidOutcome::Extracted ? 1 : 0;
		Profile.Stats.Deaths += FHollowExtractionResolver::KeepsLoadout(Outcome) ? 0 : 1;
		for (const FHollowItemStack& Sold : Delta.Sold)
		{
			Profile.Stats.CoresDelivered += Sold.Count;
		}
	}

	namespace Private
	{
		FHollowRaidReport Resolve(FHollowProfileState& State, const FHollowRaidResult& Result, const FHollowItemCatalog& Catalog)
		{
			FHollowRaidContext Context;
			Context.Loadout = State.Escrow.Loadout;
			Context.Loot = Result.Loot;
			Context.Spent = Result.Spent;
			FHollowRaidReport Report;
			Report.Result = Result;
			Report.Delta = FHollowExtractionResolver::Resolve(Context, Result.Outcome, Catalog);
			Report.CreditsBefore = State.Profile.Credits;
			Apply(State.Profile, Report.Delta, Result.Outcome);
			Report.CreditsAfter = State.Profile.Credits;
			State.Escrow = FHollowRaidEscrow();
			State.PendingReport = Report;
			State.bReportPending = true;
			return Report;
		}
	}

	TOptional<FHollowRaidReport> BeginRaid(FHollowProfileState& State, const FGuid& RaidId, const FDateTime& NowUtc, const FHollowItemCatalog& Catalog)
	{
		if (!RaidId.IsValid() || State.Escrow.RaidId == RaidId)
		{
			return TOptional<FHollowRaidReport>();
		}
		TOptional<FHollowRaidReport> Previous = ResolveEscrow(State, EHollowRaidOutcome::Abandoned, NowUtc, Catalog);
		State.Escrow.RaidId = RaidId;
		State.Escrow.Loadout = State.Profile.Loadout;
		State.Escrow.StartedUtc = NowUtc;
		return Previous;
	}

	TOptional<FHollowRaidReport> ApplyResult(FHollowProfileState& State, const FHollowRaidResult& Result, const FHollowItemCatalog& Catalog)
	{
		if (!State.Escrow.IsActive() || Result.RaidId != State.Escrow.RaidId || Result.Outcome == EHollowRaidOutcome::None)
		{
			return TOptional<FHollowRaidReport>();
		}
		return Private::Resolve(State, Result, Catalog);
	}

	TOptional<FHollowRaidReport> ResolveEscrow(FHollowProfileState& State, const EHollowRaidOutcome Outcome, const FDateTime& NowUtc,
		const FHollowItemCatalog& Catalog)
	{
		if (!State.Escrow.IsActive())
		{
			return TOptional<FHollowRaidReport>();
		}
		FHollowRaidResult Result;
		Result.RaidId = State.Escrow.RaidId;
		Result.Outcome = Outcome;
		Result.RaidSeconds = static_cast<float>(FMath::Max((NowUtc - State.Escrow.StartedUtc).GetTotalSeconds(), 0.0));
		return Private::Resolve(State, Result, Catalog);
	}

	TArray<FPrimaryAssetId> ItemsToCatalog(const FHollowProfileState& State, const FHollowRaidResult* Result)
	{
		TArray<FPrimaryAssetId> Items;
		for (const FHollowLoadoutSlot& Slot : State.Escrow.Loadout)
		{
			Items.AddUnique(Slot.Item);
		}
		for (const FHollowLoadoutSlot& Slot : State.Profile.Loadout)
		{
			Items.AddUnique(Slot.Item);
		}
		if (Result)
		{
			for (const TArray<FHollowItemStack>* Stacks : {&Result->Loot, &Result->Spent})
			{
				for (const FHollowItemStack& Stack : *Stacks)
				{
					Items.AddUnique(Stack.Item);
				}
			}
		}
		return Items;
	}
}
