#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Crafting/HollowCraftingRules.h"
#include "Extraction/HollowExtractionResolver.h"
#include "Extraction/HollowRaidTypes.h"
#include "Inventory/HollowLoadoutRules.h"
#include "Profile/HollowProfileTypes.h"
#include "Tags/HollowGameplayTags.h"

namespace HollowRelicRuleTests
{
	using R = EHollowTransactionResult;

	// Hand-made terms: invented ids, never loaded.
	const FPrimaryAssetId Cutter(TEXT("HollowItem"), TEXT("Test_Cutter"));
	const FPrimaryAssetId Lantern(TEXT("HollowItem"), TEXT("Test_Lantern"));
	const FPrimaryAssetId Welder(TEXT("HollowItem"), TEXT("Test_Welder"));
	const FPrimaryAssetId Mine(TEXT("HollowItem"), TEXT("Test_Mine"));
	const FPrimaryAssetId Ward(TEXT("HollowItem"), TEXT("Test_Ward"));
	const FPrimaryAssetId Cell(TEXT("HollowItem"), TEXT("Test_Cell"));
	const FPrimaryAssetId Ring(TEXT("HollowItem"), TEXT("Test_Ring"));
	const FPrimaryAssetId Brace(TEXT("HollowItem"), TEXT("Test_Brace"));
	const FPrimaryAssetId Reliquary(TEXT("HollowUpgrade"), TEXT("Test_Reliquary"));
	constexpr int32 ToolValue = 300;
	constexpr int32 RelicValue = 150;
	constexpr int32 GadgetValue = 40;
	constexpr int32 CellValue = 25;
	constexpr int32 GadgetStack = 3;
	constexpr int32 CellStack = 10;
	constexpr int32 MaxSlots = 6;
	constexpr int32 Credits = 1000;

	FHollowEconomyTerms MakeTerms()
	{
		FHollowEconomyTerms Terms;
		Terms.Catalog.Add(Cutter, ToolValue, true, 1, HollowTags::Item_Type_Tool, HollowTags::Item_Slot_Primary);
		Terms.Catalog.Add(Lantern, ToolValue, true, 1, HollowTags::Item_Type_Tool, HollowTags::Item_Slot_Primary);
		Terms.Catalog.Add(Welder, ToolValue, true, 1, HollowTags::Item_Type_Tool, HollowTags::Item_Slot_Secondary);
		Terms.Catalog.Add(Mine, GadgetValue, false, GadgetStack, HollowTags::Item_Type_Consumable, HollowTags::Item_Slot_Gadget);
		Terms.Catalog.Add(Ward, GadgetValue, false, GadgetStack, HollowTags::Item_Type_Consumable, HollowTags::Item_Slot_Gadget);
		Terms.Catalog.Add(Cell, CellValue, false, CellStack, HollowTags::Item_Type_Consumable);
		Terms.Catalog.Add(Ring, RelicValue, true, 1, HollowTags::Item_Type_Relic, HollowTags::Item_Slot_Relic);
		Terms.Catalog.Add(Brace, RelicValue, true, 1, HollowTags::Item_Type_Relic, HollowTags::Item_Slot_Relic);
		Terms.LoadoutItemTypes = FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>{HollowTags::Item_Type_Tool, HollowTags::Item_Type_Consumable,
			HollowTags::Item_Type_Relic});
		Terms.MaxLoadoutSlots = MaxSlots;
		Terms.MaxInsuredSlots = 2;
		Terms.InsurancePercent = 30;
		Terms.StashSlots = 40;
		for (const FGameplayTag& Slot : TArray<FGameplayTag>{HollowTags::Item_Slot_Primary, HollowTags::Item_Slot_Secondary, HollowTags::Item_Slot_Gadget,
			HollowTags::Item_Slot_Relic})
		{
			Terms.EquipSlotLimits.Add(Slot, 1);
		}
		FHollowUpgradeTerms Upgrade;
		Upgrade.Id = Reliquary;
		Upgrade.Price = 1;
		Upgrade.ExtraEquipSlots.Add({HollowTags::Item_Slot_Relic, 1});
		Terms.Upgrades.Add(Reliquary, Upgrade);
		return Terms;
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
		const FHollowItemStack* Found = Stacks.FindByPredicate([&Item](const FHollowItemStack& Each) { return Each.Item == Item; });
		return Found ? Found->Count : 0;
	}

	int32 LoadoutCount(const FHollowProfile& Profile, const FPrimaryAssetId& Item)
	{
		int32 Count = 0;
		for (const FHollowLoadoutSlot& Slot : Profile.Loadout)
		{
			Count += Slot.Item == Item ? Slot.Count : 0;
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowRelicRulesTest, "Hollow.Economy.Relics.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowRelicRulesTest::RunTest(const FString& Parameters)
{
	// ADR-0037: a loadout holds one tool a hand, one gadget, the relic slots (one, two with the Reliquary), every one-of
	// item once; a refusal changes nothing. The host drops what breaks the rule, the first one winning. Relics are
	// insured, kept and lost like any loadout slot (FHollowExtractionResolver).
	using namespace HollowRelicRuleTests;
	const FHollowEconomyTerms Terms = MakeTerms();
	FHollowProfileState State;
	State.Profile.Credits = Credits;
	State.Profile.Stash = {Stack(Cutter, 1), Stack(Lantern, 1), Stack(Welder, 2), Stack(Mine, 5), Stack(Ward, 2), Stack(Cell, 3), Stack(Ring, 2), Stack(Brace, 1)};
	auto Equip = [&State, &Terms](const FPrimaryAssetId& Item, const int32 Count)
	{
		return HollowCraftingRules::Apply(State, FHollowProfileTransaction::Equip(Item, Count), Terms);
	};

	// One tool a hand, one gadget; stackable gadgets top up.
	TestEqual(TEXT("The cutter on the primary input"), Equip(Cutter, 1), R::Applied);
	const FHollowProfileState BeforeLantern = State;
	TestEqual(TEXT("...the lantern on the same input: its slot is taken"), Equip(Lantern, 1), R::SlotTaken);
	TestTrue(TEXT("...and nothing changed"), State.Profile.Loadout == BeforeLantern.Profile.Loadout && State.Profile.Stash == BeforeLantern.Profile.Stash);
	TestEqual(TEXT("The welder on the other hand"), Equip(Welder, 1), R::Applied);
	TestEqual(TEXT("...a second welder does nothing: refused"), Equip(Welder, 1), R::SlotTaken);
	TestEqual(TEXT("Mines on the gadget input"), Equip(Mine, 2), R::Applied);
	TestEqual(TEXT("...more mines top them up"), Equip(Mine, 1), R::Applied);
	TestEqual(TEXT("...wards too would fire on G: refused"), Equip(Ward, 1), R::SlotTaken);
	TestEqual(TEXT("Fuel cells have no slot"), Equip(Cell, 3), R::Applied);

	// The relic slots: one, a second with the Reliquary; a relic once.
	TestEqual(TEXT("A relic in the relic slot"), Equip(Ring, 1), R::Applied);
	TestEqual(TEXT("...the same one again: refused"), Equip(Ring, 1), R::SlotTaken);
	TestEqual(TEXT("...another one: the only relic slot is taken"), Equip(Brace, 1), R::SlotTaken);
	State.Profile.Upgrades.Add(Reliquary);
	TestEqual(TEXT("With the Reliquary: a second relic"), Equip(Brace, 1), R::Applied);
	TestTrue(TEXT("The loadout: cutter, welder, 3 mines, 3 cells, two relics"), LoadoutCount(State.Profile, Cutter) == 1 && LoadoutCount(State.Profile, Welder) == 1
		&& LoadoutCount(State.Profile, Mine) == 3 && LoadoutCount(State.Profile, Cell) == 3 && LoadoutCount(State.Profile, Ring) == 1
		&& LoadoutCount(State.Profile, Brace) == 1 && LoadoutCount(State.Profile, Lantern) == 0 && LoadoutCount(State.Profile, Ward) == 0);

	// A relic is insured like any slot.
	const int32 RingSlot = State.Profile.Loadout.IndexOfByPredicate([](const FHollowLoadoutSlot& Slot) { return Slot.Item == Ring; });
	TestEqual(TEXT("The ring insured"), HollowCraftingRules::Apply(State, FHollowProfileTransaction::Insure(RingSlot), Terms), R::Applied);

	// The host: the first one of a slot wins, the rest is dropped.
	const TArray<FHollowItemStack> Sent = {Stack(Cutter, 1), Stack(Lantern, 1), Stack(Mine, 2), Stack(Ward, 2), Stack(Ring, 1), Stack(Ring, 1), Stack(Brace, 1)};
	TArray<FHollowLoadoutIssue> Issues;
	const TArray<FHollowItemStack> Accepted = HollowLoadoutRules::Validate(Sent, Terms.Catalog, Terms.LoadoutItemTypes, MaxSlots, Terms.EquipSlotLimits, &Issues);
	TestTrue(TEXT("Host: the cutter, the mines and the first relic"), CountOf(Accepted, Cutter) == 1 && CountOf(Accepted, Mine) == 2 && CountOf(Accepted, Ring) == 1
		&& CountOf(Accepted, Lantern) == 0 && CountOf(Accepted, Ward) == 0 && CountOf(Accepted, Brace) == 0);
	TestEqual(TEXT("...four slots dropped, their slot taken"), Issues.FilterByPredicate([](const FHollowLoadoutIssue& Issue) { return Issue.Issue == EHollowLoadoutIssue::SlotTaken; }).Num(), 4);
	const TMap<FGameplayTag, int32> WithReliquary = HollowCraftingRules::EquipSlotLimits(Terms.EquipSlotLimits,
		HollowCraftingRules::SumEffects({Reliquary}, Terms.Upgrades));
	TestEqual(TEXT("With the Reliquary the host takes both relics"), CountOf(HollowLoadoutRules::Validate(Sent, Terms.Catalog, Terms.LoadoutItemTypes, MaxSlots,
		WithReliquary), Brace), 1);
	TestEqual(TEXT("Without limits nothing is dropped for its slot (six items)"), HollowLoadoutRules::Validate(Sent, Terms.Catalog, Terms.LoadoutItemTypes, Sent.Num()).Num(), 6);

	// Kept, lost, insured: the resolver's rules, relics like any slot.
	FHollowRaidContext Raid;
	Raid.Loadout = State.Profile.Loadout;
	const FHollowProfileDelta Extracted = FHollowExtractionResolver::Resolve(Raid, EHollowRaidOutcome::Extracted, Terms.Catalog);
	TestTrue(TEXT("Extracted: both relics come home"), CountOf(Extracted.Kept, Ring) == 1 && CountOf(Extracted.Kept, Brace) == 1);
	const FHollowProfileDelta Dead = FHollowExtractionResolver::Resolve(Raid, EHollowRaidOutcome::Dead, Terms.Catalog);
	TestTrue(TEXT("Dead: the insured ring comes home, the brace is lost"), CountOf(Dead.Kept, Ring) == 1 && CountOf(Dead.Lost, Brace) == 1
		&& CountOf(Dead.Kept, Brace) == 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
