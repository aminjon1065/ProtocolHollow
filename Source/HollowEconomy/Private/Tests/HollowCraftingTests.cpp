#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Classes/HollowClassDefinition.h"
#include "Crafting/HollowCraftingRecipe.h"
#include "Crafting/HollowCraftingRules.h"
#include "Crafting/HollowEconomyConfig.h"
#include "Engine/AssetManager.h"
#include "Extraction/HollowExtractionResolver.h"
#include "Kismet/GameplayStatics.h"
#include "Profile/HollowEconomySettings.h"
#include "Profile/HollowProfileSaveGame.h"
#include "Profile/HollowProfileTypes.h"
#include "Tags/HollowGameplayTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace HollowCraftingTests
{
	// Rules data only: the shipped economy is DA_EconomyConfig and DA_Recipe_* (create_economy_assets.py).
	const FPrimaryAssetId Welder(TEXT("HollowItem"), TEXT("Test_Welder"));
	const FPrimaryAssetId Cutter(TEXT("HollowItem"), TEXT("Test_Cutter"));
	const FPrimaryAssetId Charm(TEXT("HollowItem"), TEXT("Test_Charm"));
	const FPrimaryAssetId FuelCell(TEXT("HollowItem"), TEXT("Test_FuelCell"));
	const FPrimaryAssetId Mine(TEXT("HollowItem"), TEXT("Test_Mine"));
	const FPrimaryAssetId Scrap(TEXT("HollowItem"), TEXT("Test_Scrap"));
	const FPrimaryAssetId Shard(TEXT("HollowItem"), TEXT("Test_Shard"));
	const FPrimaryAssetId Core(TEXT("HollowItem"), TEXT("Test_Core"));
	const FPrimaryAssetId Unknown(TEXT("HollowItem"), TEXT("Test_Unknown"));
	const FPrimaryAssetId CellRecipe(TEXT("HollowRecipe"), TEXT("Test_CellRecipe"));
	const FPrimaryAssetId MineRecipe(TEXT("HollowRecipe"), TEXT("Test_MineRecipe"));
	constexpr int32 WelderValue = 300;
	constexpr int32 CutterValue = 400;
	constexpr int32 CharmValue = 25;
	constexpr int32 ScrapPrice = 8;
	constexpr int32 ShardPrice = 20;
	constexpr int32 CellStack = 10;
	constexpr int32 MineStack = 3;
	constexpr int32 ScrapStack = 50;
	constexpr int32 CharmStack = 5;
	constexpr int32 CellCredits = 5;
	constexpr int32 MineCredits = 10;
	const TCHAR* const TestSlot = TEXT("HollowCraftingAutomationTest");

	FHollowItemStack Stack(const FPrimaryAssetId& Item, const int32 Count)
	{
		FHollowItemStack Result;
		Result.Item = Item;
		Result.Count = Count;
		return Result;
	}

	FHollowLoadoutSlot Slot(const FPrimaryAssetId& Item, const int32 Count, const bool bInsured = false)
	{
		FHollowLoadoutSlot Result;
		Result.Item = Item;
		Result.Count = Count;
		Result.bInsured = bInsured;
		return Result;
	}

	FHollowRecipeTerms Recipe(const FPrimaryAssetId& Id, TArray<FHollowItemStack> Inputs, const int32 Credits, const FHollowItemStack& Output)
	{
		FHollowRecipeTerms Result;
		Result.Id = Id;
		Result.Inputs = MoveTemp(Inputs);
		Result.Credits = Credits;
		Result.Output = Output;
		return Result;
	}

	/** The S5-02 economy on test items: tools and materials in the shop, a cell and a mine on the workbench, a small stash. */
	FHollowEconomyTerms MakeTerms(const int32 StashSlots = 4, const int32 LoadoutSlots = 4)
	{
		FHollowEconomyTerms Terms;
		Terms.Catalog.Add(Welder, WelderValue, /*bInsurable=*/true, 1, HollowTags::Item_Type_Tool);
		Terms.Catalog.Add(Cutter, CutterValue, /*bInsurable=*/true, 1, HollowTags::Item_Type_Tool);
		Terms.Catalog.Add(Charm, CharmValue, /*bInsurable=*/true, CharmStack, HollowTags::Item_Type_Tool);
		Terms.Catalog.Add(FuelCell, 25, /*bInsurable=*/false, CellStack, HollowTags::Item_Type_Consumable);
		Terms.Catalog.Add(Mine, 40, /*bInsurable=*/false, MineStack, HollowTags::Item_Type_Consumable);
		Terms.Catalog.Add(Scrap, ScrapPrice, /*bInsurable=*/false, ScrapStack, HollowTags::Item_Type_Material);
		Terms.Catalog.Add(Shard, ShardPrice, /*bInsurable=*/false, 20, HollowTags::Item_Type_Material);
		Terms.Catalog.Add(Core, 500, /*bInsurable=*/false, 1, HollowTags::Item_Type_AetherCore);
		Terms.ShopPrices = {{Welder, WelderValue}, {Cutter, CutterValue}, {Scrap, ScrapPrice}, {Shard, ShardPrice}};
		Terms.Recipes.Add(CellRecipe, Recipe(CellRecipe, {Stack(Scrap, 2)}, CellCredits, Stack(FuelCell, 1)));
		Terms.Recipes.Add(MineRecipe, Recipe(MineRecipe, {Stack(Scrap, 3), Stack(Shard, 1)}, MineCredits, Stack(Mine, 1)));
		Terms.SellPercent = 50;
		Terms.InsurancePercent = 30;
		Terms.MaxInsuredSlots = 2;
		Terms.StashSlots = StashSlots;
		Terms.MaxLoadoutSlots = LoadoutSlots;
		Terms.LoadoutItemTypes.AddTag(HollowTags::Item_Type_Tool);
		Terms.LoadoutItemTypes.AddTag(HollowTags::Item_Type_Consumable);
		return Terms;
	}

	int32 CountOf(const TArray<FHollowItemStack>& Stash, const FPrimaryAssetId& Item)
	{
		return HollowCraftingRules::CountOf(Stash, Item);
	}

	bool Same(const FHollowProfile& A, const FHollowProfile& B)
	{
		return A.Credits == B.Credits && A.Loadout == B.Loadout && A.Stash == B.Stash;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowCraftingBuyTest, "Hollow.Economy.Crafting.Buy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowCraftingBuyTest::RunTest(const FString& Parameters)
{
	// ADR-0031: the shop puts what it sells into the stash for its price. A stack overflows into a new stash slot; with
	// no slot left, nothing is bought and nothing paid.
	using namespace HollowCraftingTests;
	using R = EHollowTransactionResult;
	const FHollowEconomyTerms Terms = MakeTerms();
	FHollowProfile Profile;
	Profile.Credits = 100;
	TestTrue(TEXT("Ten scrap"), HollowCraftingRules::Buy(Profile, Scrap, 10, Terms) == R::Applied && Profile.Credits == 100 - 10 * ScrapPrice
		&& Profile.Stash.Num() == 1 && CountOf(Profile.Stash, Scrap) == 10);
	const FHollowProfile Before = Profile;
	TestTrue(TEXT("Not enough credits: nothing changes"), HollowCraftingRules::Buy(Profile, Scrap, 45, Terms) == R::NotEnoughCredits && Same(Profile, Before));
	TestTrue(TEXT("Consumables are not sold, only crafted"), HollowCraftingRules::Buy(Profile, FuelCell, 1, Terms) == R::NotForSale);
	TestTrue(TEXT("Unknown items"), HollowCraftingRules::Buy(Profile, Unknown, 1, Terms) == R::UnknownItem);
	TestTrue(TEXT("No count"), HollowCraftingRules::Buy(Profile, Scrap, 0, Terms) == R::BadRequest && Same(Profile, Before));

	Profile.Credits = 2000;
	TestTrue(TEXT("Past the stack: the first fills up, a second slot takes the rest"), HollowCraftingRules::Buy(Profile, Scrap, 45, Terms) == R::Applied
		&& Profile.Stash.Num() == 2 && Profile.Stash[0].Count == ScrapStack && Profile.Stash[1].Count == 5);
	HollowCraftingRules::Buy(Profile, Shard, 3, Terms);
	HollowCraftingRules::Buy(Profile, Welder, 1, Terms);
	TestEqual(TEXT("Four stash slots used"), Profile.Stash.Num(), 4);
	const FHollowProfile Full = Profile;
	TestTrue(TEXT("No slot for a cutter: refused, not paid"), HollowCraftingRules::Buy(Profile, Cutter, 1, Terms) == R::StashFull && Same(Profile, Full));
	TestTrue(TEXT("Scrap still fits its open stack"), HollowCraftingRules::Buy(Profile, Scrap, 45, Terms) == R::Applied && Profile.Stash.Num() == 4
		&& CountOf(Profile.Stash, Scrap) == 2 * ScrapStack);
	TestTrue(TEXT("...and then no more"), HollowCraftingRules::Buy(Profile, Scrap, 1, Terms) == R::StashFull);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowCraftingSellTest, "Hollow.Economy.Crafting.Sell",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowCraftingSellTest::RunTest(const FString& Parameters)
{
	// The corporation pays half the value, rounded down, for anything in the stash; never more than is there.
	using namespace HollowCraftingTests;
	using R = EHollowTransactionResult;
	const FHollowEconomyTerms Terms = MakeTerms();
	FHollowProfile Profile;
	Profile.Stash = {Stack(Scrap, ScrapStack), Stack(Shard, 3), Stack(Scrap, 5), Stack(FuelCell, 3)};
	TestEqual(TEXT("Half the value"), HollowCraftingRules::SellPrice(Terms, Welder), WelderValue / 2);
	TestEqual(TEXT("...rounded down (25 -> 12)"), HollowCraftingRules::SellPrice(Terms, FuelCell), 12);
	TestTrue(TEXT("Fifty-two scrap: from the last stacks first"), HollowCraftingRules::Sell(Profile, Scrap, 52, Terms) == R::Applied
		&& Profile.Credits == 52 * (ScrapPrice / 2) && CountOf(Profile.Stash, Scrap) == 3 && Profile.Stash.Num() == 3);
	const FHollowProfile Before = Profile;
	TestTrue(TEXT("More shards than there are: nothing sold"), HollowCraftingRules::Sell(Profile, Shard, 4, Terms) == R::NotEnoughItems && Same(Profile, Before));
	TestTrue(TEXT("Nothing to sell of a core"), HollowCraftingRules::Sell(Profile, Core, 1, Terms) == R::NotEnoughItems);
	TestTrue(TEXT("A crafted cell sells too"), HollowCraftingRules::Sell(Profile, FuelCell, 3, Terms) == R::Applied && CountOf(Profile.Stash, FuelCell) == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowCraftingCraftTest, "Hollow.Economy.Crafting.Craft",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowCraftingCraftTest::RunTest(const FString& Parameters)
{
	// The workbench takes a recipe's inputs and credits from the stash and puts its output there, all or nothing: missing
	// inputs or credits, or no room for the output, change nothing. Inputs freed make room for the output.
	using namespace HollowCraftingTests;
	using R = EHollowTransactionResult;
	const FHollowEconomyTerms Terms = MakeTerms();
	FHollowProfile Profile;
	Profile.Credits = 100;
	Profile.Stash = {Stack(Scrap, 6), Stack(Shard, 1)};
	TestTrue(TEXT("A mine: 3 scrap, a shard, 10 credits"), HollowCraftingRules::Craft(Profile, MineRecipe, 1, Terms) == R::Applied
		&& CountOf(Profile.Stash, Scrap) == 3 && CountOf(Profile.Stash, Shard) == 0 && CountOf(Profile.Stash, Mine) == 1 && Profile.Credits == 100 - MineCredits);
	TestTrue(TEXT("...its used-up stack is gone"), Profile.Stash.Num() == 2);
	FHollowProfile Before = Profile;
	TestTrue(TEXT("No shard left: no second mine"), HollowCraftingRules::Craft(Profile, MineRecipe, 1, Terms) == R::NotEnoughItems && Same(Profile, Before));
	TestTrue(TEXT("Two cells need 4 scrap: none crafted"), HollowCraftingRules::Craft(Profile, CellRecipe, 2, Terms) == R::NotEnoughItems && Same(Profile, Before));
	TestTrue(TEXT("One cell"), HollowCraftingRules::Craft(Profile, CellRecipe, 1, Terms) == R::Applied && CountOf(Profile.Stash, Scrap) == 1
		&& CountOf(Profile.Stash, FuelCell) == 1 && Profile.Credits == 100 - MineCredits - CellCredits);
	Profile.Credits = CellCredits - 1;
	Profile.Stash.Add(Stack(Scrap, 10));
	Before = Profile;
	TestTrue(TEXT("Not enough credits"), HollowCraftingRules::Craft(Profile, CellRecipe, 1, Terms) == R::NotEnoughCredits && Same(Profile, Before));
	TestTrue(TEXT("Unknown recipe"), HollowCraftingRules::Craft(Profile, FPrimaryAssetId(TEXT("HollowRecipe"), TEXT("Test_None")), 1, Terms) == R::UnknownRecipe);
	TestTrue(TEXT("No times"), HollowCraftingRules::Craft(Profile, CellRecipe, 0, Terms) == R::BadRequest);

	// Stack overflow on two stash slots: the scrap used up frees the slot the overflowing cells need...
	const FHollowEconomyTerms TwoSlots = MakeTerms(/*StashSlots=*/2);
	Profile.Credits = 100;
	Profile.Stash = {Stack(FuelCell, CellStack - 1), Stack(Scrap, 4)};
	TestTrue(TEXT("Two cells onto nine: a full stack and a new one where the scrap was"), HollowCraftingRules::Craft(Profile, CellRecipe, 2, TwoSlots) == R::Applied
		&& Profile.Stash.Num() == 2 && Profile.Stash[0].Count == CellStack && Profile.Stash[1].Item == FuelCell && Profile.Stash[1].Count == 1);
	// ...but with scrap left over there is no room: nothing is taken.
	Profile.Stash = {Stack(FuelCell, CellStack), Stack(Scrap, ScrapStack)};
	Before = Profile;
	TestTrue(TEXT("No room for the output: no scrap taken, no credits"), HollowCraftingRules::Craft(Profile, CellRecipe, 1, TwoSlots) == R::StashFull && Same(Profile, Before));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowCraftingInsuranceTest, "Hollow.Economy.Crafting.Insurance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowCraftingInsuranceTest::RunTest(const FString& Parameters)
{
	// Insurance of a loadout slot costs 30 % of its items' value, rounded up, for insurable items, at most 2 slots. It is
	// single-use: spent when it saves the items (death), kept when the player comes back (extraction).
	using namespace HollowCraftingTests;
	using R = EHollowTransactionResult;
	const FHollowEconomyTerms Terms = MakeTerms();
	FHollowProfile Profile;
	Profile.Credits = 500;
	Profile.Loadout = {Slot(Welder, 1), Slot(Cutter, 1), Slot(FuelCell, 3), Slot(Charm, 1)};
	TestEqual(TEXT("30 % of the value"), HollowCraftingRules::InsurancePrice(Terms, Profile.Loadout[1]), CutterValue * 30 / 100);
	TestEqual(TEXT("...rounded up (7.5 -> 8)"), HollowCraftingRules::InsurancePrice(Terms, Profile.Loadout[3]), 8);
	TestTrue(TEXT("The welder insured"), HollowCraftingRules::Insure(Profile, 0, Terms) == R::Applied && Profile.Loadout[0].bInsured && Profile.Credits == 500 - 90);
	TestTrue(TEXT("...once"), HollowCraftingRules::Insure(Profile, 0, Terms) == R::AlreadyInsured);
	TestTrue(TEXT("Fuel cannot be insured"), HollowCraftingRules::Insure(Profile, 2, Terms) == R::NotInsurable);
	TestTrue(TEXT("No such slot"), HollowCraftingRules::Insure(Profile, 9, Terms) == R::BadRequest);
	FHollowProfile Poor = Profile;
	Poor.Credits = 10;
	TestTrue(TEXT("Not enough credits"), HollowCraftingRules::Insure(Poor, 1, Terms) == R::NotEnoughCredits && !Poor.Loadout[1].bInsured && Poor.Credits == 10);
	TestTrue(TEXT("The cutter insured"), HollowCraftingRules::Insure(Profile, 1, Terms) == R::Applied && Profile.Credits == 500 - 90 - 120);
	TestTrue(TEXT("A third slot: over the limit"), HollowCraftingRules::Insure(Profile, 3, Terms) == R::InsuranceLimit && !Profile.Loadout[3].bInsured);

	// Single-use: a death keeps both insured tools and spends their insurance; coming back keeps it.
	FHollowRaidContext Raid;
	Raid.Loadout = Profile.Loadout;
	const FHollowProfileDelta Death = FHollowExtractionResolver::Resolve(Raid, EHollowRaidOutcome::Dead, Terms.Catalog);
	TestTrue(TEXT("Dead: the insured tools are kept, their insurance used"), Death.Kept.Num() == 2 && Death.InsuranceUsed.Num() == 2);
	FHollowProfile AfterDeath = Profile;
	HollowProfileRules::Apply(AfterDeath, Death, EHollowRaidOutcome::Dead);
	TestTrue(TEXT("...and are no longer insured"), AfterDeath.Loadout.Num() == 2 && !AfterDeath.Loadout[0].bInsured && !AfterDeath.Loadout[1].bInsured);
	TestTrue(TEXT("...so they can be insured again"), HollowCraftingRules::Insure(AfterDeath, 0, Terms) == R::Applied);
	const FHollowProfileDelta Home = FHollowExtractionResolver::Resolve(Raid, EHollowRaidOutcome::Extracted, Terms.Catalog);
	FHollowProfile AfterHome = Profile;
	HollowProfileRules::Apply(AfterHome, Home, EHollowRaidOutcome::Extracted);
	TestTrue(TEXT("Extracted: the insurance stays"), Home.InsuranceUsed.IsEmpty() && AfterHome.Loadout[0].bInsured && AfterHome.Loadout[1].bInsured);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowCraftingEquipTest, "Hollow.Economy.Crafting.Equip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowCraftingEquipTest::RunTest(const FString& Parameters)
{
	// The loadout locker: stash -> loadout fills open uninsured slots first, then new slots of at most a stack, within
	// the loadout's slots, raid items only; loadout -> stash, a whole slot with its insurance.
	using namespace HollowCraftingTests;
	using R = EHollowTransactionResult;
	const FHollowEconomyTerms Terms = MakeTerms(/*StashSlots=*/6, /*LoadoutSlots=*/4);
	FHollowProfile Profile;
	Profile.Loadout = {Slot(Welder, 1, true), Slot(Mine, 2)};
	Profile.Stash = {Stack(Mine, 5), Stack(FuelCell, CellStack), Stack(FuelCell, 2), Stack(Scrap, 10), Stack(Charm, 3)};
	TestTrue(TEXT("Two mines: the open slot fills, a new one takes the other"), HollowCraftingRules::Equip(Profile, Mine, 2, Terms) == R::Applied
		&& Profile.Loadout.Num() == 3 && Profile.Loadout[1].Count == MineStack && Profile.Loadout[2] == Slot(Mine, 1) && CountOf(Profile.Stash, Mine) == 3);
	TestTrue(TEXT("Materials stay home"), HollowCraftingRules::Equip(Profile, Scrap, 1, Terms) == R::NotForRaids);
	FHollowProfile Before = Profile;
	TestTrue(TEXT("Twelve cells need two slots, one is left: nothing moves"), HollowCraftingRules::Equip(Profile, FuelCell, 12, Terms) == R::LoadoutFull && Same(Profile, Before));
	TestTrue(TEXT("More than the stash holds"), HollowCraftingRules::Equip(Profile, Mine, 10, Terms) == R::NotEnoughItems && Same(Profile, Before));
	TestTrue(TEXT("Ten cells: the last slot"), HollowCraftingRules::Equip(Profile, FuelCell, CellStack, Terms) == R::Applied && Profile.Loadout.Num() == 4
		&& CountOf(Profile.Stash, FuelCell) == 2);

	// An insured slot never grows for free.
	FHollowProfile Charmed;
	Charmed.Loadout = {Slot(Charm, 2, true)};
	Charmed.Stash = {Stack(Charm, 2)};
	TestTrue(TEXT("More charms go to a new slot, not into the insured one"), HollowCraftingRules::Equip(Charmed, Charm, 2, Terms) == R::Applied
		&& Charmed.Loadout.Num() == 2 && Charmed.Loadout[0] == Slot(Charm, 2, true) && Charmed.Loadout[1] == Slot(Charm, 2));
	TestTrue(TEXT("Part of a slot back: the slot and its insurance stay"), HollowCraftingRules::Unequip(Charmed, 0, 1, Terms) == R::Applied
		&& Charmed.Loadout[0] == Slot(Charm, 1, true) && CountOf(Charmed.Stash, Charm) == 1);
	TestTrue(TEXT("The whole slot back: gone, with its insurance"), HollowCraftingRules::Unequip(Charmed, 0, 0, Terms) == R::Applied
		&& Charmed.Loadout.Num() == 1 && !Charmed.Loadout[0].bInsured && CountOf(Charmed.Stash, Charm) == 2);
	TestTrue(TEXT("More than the slot holds"), HollowCraftingRules::Unequip(Charmed, 0, 3, Terms) == R::NotEnoughItems);
	TestTrue(TEXT("No such slot"), HollowCraftingRules::Unequip(Charmed, 5, 0, Terms) == R::BadRequest);
	FHollowProfile Crowded;
	Crowded.Loadout = {Slot(Welder, 1)};
	Crowded.Stash = {Stack(Scrap, 1), Stack(Scrap, 1), Stack(Scrap, 1), Stack(Scrap, 1), Stack(Scrap, 1), Stack(Scrap, 1)};
	Before = Crowded;
	TestTrue(TEXT("No stash slot for it: the loadout keeps it"), HollowCraftingRules::Unequip(Crowded, 0, 0, Terms) == R::StashFull && Same(Crowded, Before));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowCraftingTransactionsTest, "Hollow.Economy.Crafting.Transactions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowCraftingTransactionsTest::RunTest(const FString& Parameters)
{
	// Idempotent and safe around raids: a transaction sent twice applies once; refused ones are not remembered (they may
	// be sent again); none applies while a raid is in escrow; the last 32 ids are remembered, on disk too.
	using namespace HollowCraftingTests;
	using R = EHollowTransactionResult;
	const FHollowEconomyTerms Terms = MakeTerms(/*StashSlots=*/40);
	FHollowProfileState State;
	State.Profile.Credits = 100;
	const FHollowProfileTransaction Buy = FHollowProfileTransaction::Buy(Scrap, 2);
	TestTrue(TEXT("Applied"), HollowCraftingRules::Apply(State, Buy, Terms) == R::Applied && State.Profile.Credits == 100 - 2 * ScrapPrice);
	TestTrue(TEXT("The same transaction again changes nothing"), HollowCraftingRules::Apply(State, Buy, Terms) == R::AlreadyApplied
		&& State.Profile.Credits == 100 - 2 * ScrapPrice && CountOf(State.Profile.Stash, Scrap) == 2);
	TestTrue(TEXT("A new one applies"), HollowCraftingRules::Apply(State, FHollowProfileTransaction::Buy(Scrap, 2), Terms) == R::Applied && CountOf(State.Profile.Stash, Scrap) == 4);
	FHollowProfileTransaction NoId = FHollowProfileTransaction::Buy(Scrap, 1);
	NoId.Id.Invalidate();
	TestTrue(TEXT("Without an id: refused"), HollowCraftingRules::Apply(State, NoId, Terms) == R::BadRequest);

	const FHollowProfileTransaction Dear = FHollowProfileTransaction::Buy(Cutter, 1);
	TestTrue(TEXT("Refused for credits..."), HollowCraftingRules::Apply(State, Dear, Terms) == R::NotEnoughCredits);
	State.Profile.Credits += CutterValue;
	TestTrue(TEXT("...the same request applies once it can"), HollowCraftingRules::Apply(State, Dear, Terms) == R::Applied && CountOf(State.Profile.Stash, Cutter) == 1);

	State.Escrow.RaidId = FGuid::NewGuid();
	const FHollowProfile Untouched = State.Profile;
	const FHollowProfileTransaction Sell = FHollowProfileTransaction::Sell(Scrap, 1);
	TestTrue(TEXT("In a raid: refused, nothing changes"), HollowCraftingRules::Apply(State, Sell, Terms) == R::InRaid && Same(State.Profile, Untouched));
	State.Escrow = FHollowRaidEscrow();
	TestTrue(TEXT("...back in the hub it applies"), HollowCraftingRules::Apply(State, Sell, Terms) == R::Applied);

	const FHollowProfileTransaction First = FHollowProfileTransaction::Buy(Scrap, 1);
	State.Profile.Credits = 1000;
	HollowCraftingRules::Apply(State, First, Terms);
	for (int32 Index = 0; Index < HollowCraftingRules::RememberedTransactions; ++Index)
	{
		HollowCraftingRules::Apply(State, FHollowProfileTransaction::Buy(Scrap, 1), Terms);
	}
	TestEqual(TEXT("The last 32 are remembered"), State.RecentTransactions.Num(), HollowCraftingRules::RememberedTransactions);
	TestFalse(TEXT("...the oldest forgotten"), State.RecentTransactions.Contains(First.Id));

	UHollowProfileSaveGame* Save = Cast<UHollowProfileSaveGame>(UGameplayStatics::CreateSaveGameObject(UHollowProfileSaveGame::StaticClass()));
	Save->State = State;
	if (!TestTrue(TEXT("Saved"), UGameplayStatics::SaveGameToSlot(Save, TestSlot, 0)))
	{
		return false;
	}
	const UHollowProfileSaveGame* Loaded = Cast<UHollowProfileSaveGame>(UGameplayStatics::LoadGameFromSlot(TestSlot, 0));
	UGameplayStatics::DeleteGameInSlot(TestSlot, 0);
	TestTrue(TEXT("On disk: the stash and the remembered ids"), Loaded && Loaded->State.Profile.Stash == State.Profile.Stash
		&& Loaded->State.RecentTransactions == State.RecentTransactions);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowCraftingDataTest, "Hollow.Economy.Crafting.Data",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowCraftingDataTest::RunTest(const FString& Parameters)
{
	// The shipped economy (DA_EconomyConfig, DA_Recipe_*): valid; the shop sells no consumables; no loop of buying,
	// crafting and selling earns credits; a new profile crafts a pulse mine and a fuel cell from its stash and takes
	// them into its loadout.
	using namespace HollowCraftingTests;
	using R = EHollowTransactionResult;
	const UHollowEconomyConfig* Config = UHollowEconomyConfig::Get();
	if (!TestNotNull(TEXT("DA_EconomyConfig"), Config))
	{
		return false;
	}
#if WITH_EDITOR   // Data Validation exists in editor builds only
	FDataValidationContext Validation;
	TestTrue(TEXT("The config is valid"), Config->IsDataValid(Validation) != EDataValidationResult::Invalid && Validation.GetNumErrors() == 0);
#endif
	const UHollowEconomySettings* Settings = GetDefault<UHollowEconomySettings>();
	FHollowProfileState State;
	State.Profile.Credits = Settings->StartingCredits;
	const UHollowClassDefinition* DefaultClass = UHollowClassDefinition::Load(Settings->DefaultClass);
	State.Profile.Loadout = DefaultClass ? DefaultClass->StartingLoadout : TArray<FHollowLoadoutSlot>();
	State.Profile.Stash = Settings->StartingStash;
	const FHollowEconomyTerms Terms = FHollowEconomyTerms::Load(State);
	const FPrimaryAssetId CellRecipeId(TEXT("HollowRecipe"), TEXT("DA_Recipe_FuelCell"));
	const FPrimaryAssetId MineRecipeId(TEXT("HollowRecipe"), TEXT("DA_Recipe_PulseMine"));
	TestTrue(TEXT("Both recipes found"), Terms.Recipes.Contains(CellRecipeId) && Terms.Recipes.Contains(MineRecipeId));
	for (const TPair<FPrimaryAssetId, int32>& Offer : Terms.ShopPrices)
	{
		TestFalse(Offer.Key.ToString() + TEXT(" is no consumable (those come from the workbench)"),
			Terms.Catalog.GetItemType(Offer.Key).MatchesTag(HollowTags::Item_Type_Consumable));
		TestTrue(Offer.Key.ToString() + TEXT(": buying to sell never pays"), HollowCraftingRules::SellPrice(Terms, Offer.Key) < Offer.Value);
	}
	for (const TPair<FPrimaryAssetId, FHollowRecipeTerms>& Pair : Terms.Recipes)
	{
		const FHollowRecipeTerms& Recipe = Pair.Value;
		int64 Cost = Recipe.Credits;
		for (const FHollowItemStack& Input : Recipe.Inputs)
		{
			const int32* Price = Terms.ShopPrices.Find(Input.Item);
			TestTrue(Input.Item.ToString() + TEXT(" is a material from the shop"), Price && Terms.Catalog.GetItemType(Input.Item).MatchesTag(HollowTags::Item_Type_Material));
			Cost += Price ? static_cast<int64>(*Price) * Input.Count : 0;
		}
		TestTrue(Recipe.Id.ToString() + TEXT(" makes a consumable or a relic (S6-04)"), Terms.Catalog.GetItemType(Recipe.Output.Item).MatchesTag(HollowTags::Item_Type_Consumable)
			|| Terms.Catalog.GetItemType(Recipe.Output.Item).MatchesTag(HollowTags::Item_Type_Relic));
		TestTrue(Recipe.Id.ToString() + TEXT(": crafting to sell never pays"), static_cast<int64>(HollowCraftingRules::SellPrice(Terms, Recipe.Output.Item)) * Recipe.Output.Count < Cost);
#if WITH_EDITOR
		const UHollowCraftingRecipe* Asset = Cast<UHollowCraftingRecipe>(UAssetManager::Get().GetPrimaryAssetPath(Recipe.Id).TryLoad());
		FDataValidationContext RecipeValidation;
		TestTrue(Recipe.Id.ToString() + TEXT(" is valid"), Asset && Asset->IsDataValid(RecipeValidation) != EDataValidationResult::Invalid);
#endif
	}
	const FPrimaryAssetId MineItem(TEXT("HollowItem"), TEXT("DA_Item_PulseMine"));
	const auto MinesTaken = [&State, &MineItem]
	{
		int32 Count = 0;
		for (const FHollowLoadoutSlot& Slot : State.Profile.Loadout)
		{
			Count += Slot.Item == MineItem ? Slot.Count : 0;
		}
		return Count;
	};
	TestTrue(TEXT("A new profile crafts a pulse mine"), HollowCraftingRules::Apply(State, FHollowProfileTransaction::Craft(MineRecipeId, 1), Terms) == R::Applied);
	TestTrue(TEXT("...and a fuel cell"), HollowCraftingRules::Apply(State, FHollowProfileTransaction::Craft(CellRecipeId, 1), Terms) == R::Applied);
	const int32 MinesBefore = MinesTaken();
	TestTrue(TEXT("...and takes the mine along"), HollowCraftingRules::Apply(State, FHollowProfileTransaction::Equip(MineItem, 1), Terms) == R::Applied
		&& HollowCraftingRules::CountOf(State.Profile.Stash, MineItem) == 0 && MinesTaken() == MinesBefore + 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
