#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Crafting/HollowCraftingRules.h"
#include "Extraction/HollowExtractionResolver.h"
#include "Extraction/HollowRaidTypes.h"
#include "Loot/HollowLootTable.h"
#include "Profile/HollowProfileTypes.h"
#include "Tags/HollowGameplayTags.h"

namespace HollowLootRuleTests
{
	// Hand-made catalog and rows: invented ids, never loaded.
	const FPrimaryAssetId Scrap(TEXT("HollowItem"), TEXT("Test_Scrap"));
	const FPrimaryAssetId Shard(TEXT("HollowItem"), TEXT("Test_Shard"));
	const FPrimaryAssetId Nails(TEXT("HollowItem"), TEXT("Test_Nails"));
	const FPrimaryAssetId Cutter(TEXT("HollowItem"), TEXT("Test_Cutter"));
	constexpr int32 ScrapValue = 8;
	constexpr int32 ScrapStack = 50;
	constexpr int32 Seeds = 2000;

	FHollowLootRow Row(const FPrimaryAssetId& Item, const int32 Weight, const int32 Low, const int32 High)
	{
		FHollowLootRow Result;
		Result.Item = Item;
		Result.Weight = Weight;
		Result.MinCount = Low;
		Result.MaxCount = High;
		return Result;
	}

	FHollowItemCatalog Catalog()
	{
		FHollowItemCatalog Result;
		Result.Add(Scrap, ScrapValue, false, ScrapStack, HollowTags::Item_Type_Material);
		Result.Add(Shard, 20, false, 20, HollowTags::Item_Type_Material);
		Result.Add(Nails, 1, false, 60, HollowTags::Item_Type_Ammo);
		Result.Add(Cutter, 400, true, 1, HollowTags::Item_Type_Tool, HollowTags::Item_Slot_Primary);
		return Result;
	}

	FHollowItemStack Stack(const FPrimaryAssetId& Item, const int32 Count)
	{
		FHollowItemStack Result;
		Result.Item = Item;
		Result.Count = Count;
		return Result;
	}

	int32 CountOf(const TArray<FHollowItemStack>& Stacks, const FPrimaryAssetId& Item)
	{
		int32 Total = 0;
		for (const FHollowItemStack& Entry : Stacks)
		{
			Total += Entry.Item == Item ? Entry.Count : 0;
		}
		return Total;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowLootRollTest, "Hollow.Economy.Loot.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowLootRollTest::RunTest(const FString& Parameters)
{
	// GP-01b (ADR-0045): a crate's roll is the same for the same seed, within the table's bounds, by its weights; the
	// found loot held at the end: what was taken from the profile is covered first.
	using namespace HollowLootRuleTests;
	using HollowLootRules::Roll;
	const TArray<FHollowLootRow> Rows = {Row(Scrap, 3, 2, 5), Row(Shard, 1, 1, 1)};
	TestTrue(TEXT("The same seed: the same crate"), Roll(Rows, 1, 3, 77) == Roll(Rows, 1, 3, 77));
	int32 Different = 0;
	int32 ScrapRolls = 0;
	int32 ShardRolls = 0;
	bool bInBounds = true;
	for (int32 Seed = 0; Seed < Seeds; ++Seed)
	{
		const TArray<FHollowItemStack> Crate = Roll(Rows, 1, 1, Seed);
		Different += Crate != Roll(Rows, 1, 1, Seed + Seeds) ? 1 : 0;
		bInBounds &= Crate.Num() == 1;
		for (const FHollowItemStack& Entry : Crate)
		{
			if (Entry.Item == Scrap)
			{
				++ScrapRolls;
				bInBounds &= Entry.Count >= 2 && Entry.Count <= 5;
			}
			else
			{
				++ShardRolls;
				bInBounds &= Entry.Item == Shard && Entry.Count == 1;
			}
		}
	}
	TestTrue(TEXT("One drop each, every count within its row"), bInBounds);
	TestTrue(TEXT("Other seeds: other crates"), Different > Seeds / 4);
	const float Share = static_cast<float>(ScrapRolls) / FMath::Max(ScrapRolls + ShardRolls, 1);
	TestTrue(FString::Printf(TEXT("By weight: 3 to 1 (%.2f scrap)"), Share), Share > 0.7f && Share < 0.8f);
	const TArray<FHollowItemStack> Many = Roll({Row(Scrap, 1, 1, 1)}, 4, 4, 5);
	TestTrue(TEXT("The same item rolled again adds up"), Many.Num() == 1 && Many[0].Count == 4);
	TestTrue(TEXT("Nothing to roll: nothing"), Roll({}, 1, 3, 5).IsEmpty() && Roll(Rows, 0, 0, 5).IsEmpty());

	using HollowLootRules::FoundHeld;
	TestEqual(TEXT("Found and held, nothing used"), FoundHeld(30, 10, 20), 20);
	TestEqual(TEXT("Some used: the found go before the own"), FoundHeld(22, 10, 20), 12);
	TestEqual(TEXT("More used than found: none of it left"), FoundHeld(8, 10, 20), 0);
	TestEqual(TEXT("Free items on top: never counted as found"), FoundHeld(35, 10, 20), 20);
	TestEqual(TEXT("Nothing found"), FoundHeld(12, 10, 0), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowResolverFoundTest, "Hollow.Economy.Resolver.Found",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowResolverFoundTest::RunTest(const FString& Parameters)
{
	// GP-01b: the player's found loot comes home into the stash when extracted (what does not fit is sold), and is left
	// behind in every other outcome; the squad's cores and the loadout are resolved as before.
	using namespace HollowLootRuleTests;
	const FHollowItemCatalog Items = Catalog();
	FHollowRaidContext Context;
	Context.Found = {Stack(Scrap, 12), Stack(Nails, 30)};
	Context.Stash = {Stack(Scrap, 45), Stack(Shard, 3)};
	Context.StashSlots = 4;
	Context.SellPercent = 50;

	TestEqual(TEXT("Room in the stash: the partial stack and the free slots"), HollowCraftingRules::RoomInStash(Context.Stash, Scrap, ScrapStack, 4),
		static_cast<int64>(5 + 2 * ScrapStack));
	TestEqual(TEXT("...no limit without slots"), HollowCraftingRules::RoomInStash(Context.Stash, Scrap, ScrapStack, 0), static_cast<int64>(MAX_int32));

	FHollowProfileDelta Delta = FHollowExtractionResolver::Resolve(Context, EHollowRaidOutcome::Extracted, Items);
	TestEqual(TEXT("Extracted: the scrap into the stash"), CountOf(Delta.Found, Scrap), 12);
	TestEqual(TEXT("...the nails too (a new slot)"), CountOf(Delta.Found, Nails), 30);
	TestTrue(TEXT("...nothing sold, nothing lost, no credits for it"), Delta.FoundSold.IsEmpty() && Delta.FoundLost.IsEmpty() && Delta.Credits == 0);

	Context.StashSlots = 2;   // full but for the scrap's partial stack
	Delta = FHollowExtractionResolver::Resolve(Context, EHollowRaidOutcome::Extracted, Items);
	TestEqual(TEXT("A full stash: what fits goes in"), CountOf(Delta.Found, Scrap), 5);
	TestEqual(TEXT("...the rest of the scrap sold"), CountOf(Delta.FoundSold, Scrap), 7);
	TestEqual(TEXT("...the nails sold"), CountOf(Delta.FoundSold, Nails), 30);
	TestEqual(TEXT("...at the sell price of one, rounded down (a nail: nothing)"), Delta.Credits, 7 * (ScrapValue * 50 / 100) + 30 * (1 * 50 / 100));

	for (const EHollowRaidOutcome Outcome : {EHollowRaidOutcome::Dead, EHollowRaidOutcome::MissingInAction, EHollowRaidOutcome::SignalLost,
		EHollowRaidOutcome::Abandoned})
	{
		Delta = FHollowExtractionResolver::Resolve(Context, Outcome, Items);
		TestTrue(FString::Printf(TEXT("Outcome %d: the found loot left behind"), static_cast<int32>(Outcome)),
			Delta.Found.IsEmpty() && CountOf(Delta.FoundLost, Scrap) == 12 && CountOf(Delta.FoundLost, Nails) == 30 && Delta.Credits == 0);
	}

	// Through the profile: the stash grows by stacks of the catalog's size.
	FHollowProfileState State;
	State.Profile.Stash = {Stack(Scrap, 45)};
	State.Profile.Loadout = {FHollowLoadoutSlot()};
	State.Profile.Loadout[0].Item = Cutter;
	State.Profile.Loadout[0].Count = 1;
	HollowProfileRules::BeginRaid(State, FGuid::NewGuid(), FDateTime::UtcNow(), Items);
	FHollowRaidResult Result;
	Result.RaidId = State.Escrow.RaidId;
	Result.Outcome = EHollowRaidOutcome::Extracted;
	Result.Found = {Stack(Scrap, 12)};
	const TOptional<FHollowRaidReport> Report = HollowProfileRules::ApplyResult(State, Result, Items, 0, 40, 50);
	TestTrue(TEXT("The result resolves"), Report.IsSet() && CountOf(Report->Delta.Found, Scrap) == 12);
	TestTrue(TEXT("The stash: a full stack and the rest"), State.Profile.Stash.Num() == 2 && State.Profile.Stash[0].Count == ScrapStack
		&& State.Profile.Stash[1].Item == Scrap && State.Profile.Stash[1].Count == 7);
	TestTrue(TEXT("...the loadout as before"), State.Profile.Loadout.Num() == 1 && State.Profile.Loadout[0].Item == Cutter);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
