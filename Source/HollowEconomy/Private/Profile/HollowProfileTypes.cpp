#include "Profile/HollowProfileTypes.h"

#include "Crafting/HollowCraftingRules.h"
#include "Extraction/HollowExtractionResolver.h"
#include "Items/HollowItemTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowProfileTypes)

namespace HollowProfileRules
{
	void Apply(FHollowProfile& Profile, const FHollowProfileDelta& Delta, const EHollowRaidOutcome Outcome, const FHollowItemCatalog* Catalog)
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
		// Insurance is single-use (ADR-0031): what it saved, it no longer covers.
		for (const FHollowItemStack& Saved : Delta.InsuranceUsed)
		{
			for (FHollowLoadoutSlot& Slot : Profile.Loadout)
			{
				if (Slot.bInsured && Slot.Item == Saved.Item)
				{
					Slot.bInsured = false;
				}
			}
		}

		// GP-01b: the found loot, into the stash (the resolver kept to its room).
		for (const FHollowItemStack& Found : Delta.Found)
		{
			const int32 MaxStack = Catalog ? Catalog->GetMaxStack(Found.Item) : FMath::Max(Found.Count, 1);
			HollowCraftingRules::AddToStash(Profile.Stash, Found.Item, Found.Count, MaxStack, MAX_int32);
		}

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
		FHollowRaidReport Resolve(FHollowProfileState& State, const FHollowRaidResult& Result, const FHollowItemCatalog& Catalog,
			const int32 CoreValuePercent = 0, const int32 StashSlots = 0, const int32 SellPercent = 0, const int32 LoreBonus = 0)
		{
			FHollowRaidContext Context;
			Context.CoreValuePercent = CoreValuePercent;
			Context.Loadout = State.Escrow.Loadout;
			Context.Loot = Result.Loot;
			Context.Spent = Result.Spent;
			Context.Found = Result.Found;
			Context.Stash = State.Profile.Stash;
			Context.StashSlots = StashSlots;
			Context.SellPercent = SellPercent;
			FHollowRaidReport Report;
			Report.Result = Result;
			Report.Delta = FHollowExtractionResolver::Resolve(Context, Result.Outcome, Catalog);
			// NAR-01b: the crew logs stay found whatever the end; their bonus only comes home with the player.
			Report.Delta.Lore = State.Escrow.NewLore;
			if (Result.Outcome == EHollowRaidOutcome::Extracted)
			{
				Report.Delta.LoreCredits = FMath::Max(LoreBonus, 0) * State.Escrow.NewLore.Num();
				Report.Delta.Credits += Report.Delta.LoreCredits;
			}
			Report.CreditsBefore = State.Profile.Credits;
			Apply(State.Profile, Report.Delta, Result.Outcome, &Catalog);
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

	TOptional<FHollowRaidReport> ApplyResult(FHollowProfileState& State, const FHollowRaidResult& Result, const FHollowItemCatalog& Catalog,
		const int32 CoreValuePercent, const int32 StashSlots, const int32 SellPercent, const int32 LoreBonus)
	{
		if (!State.Escrow.IsActive() || Result.RaidId != State.Escrow.RaidId || Result.Outcome == EHollowRaidOutcome::None)
		{
			return TOptional<FHollowRaidReport>();
		}
		return Private::Resolve(State, Result, Catalog, CoreValuePercent, StashSlots, SellPercent, LoreBonus);
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

	bool AddLore(FHollowProfileState& State, const FPrimaryAssetId& Entry)
	{
		if (!Entry.IsValid() || State.Profile.Lore.Contains(Entry))
		{
			return false;
		}
		State.Profile.Lore.Add(Entry);
		if (State.Escrow.IsActive())
		{
			State.Escrow.NewLore.AddUnique(Entry);
		}
		return true;
	}

	bool MarkRadioHeard(FHollowProfile& Profile, const FName CueId)
	{
		if (CueId.IsNone() || Profile.RadioHeard.Contains(CueId))
		{
			return false;
		}
		Profile.RadioHeard.Add(CueId);
		return true;
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
			for (const TArray<FHollowItemStack>* Stacks : {&Result->Loot, &Result->Spent, &Result->Found})
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
