#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/HollowGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "Crafting/HollowCraftingRules.h"
#include "Crafting/HollowShipUpgradeDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "Extraction/HollowExtractionResolver.h"
#include "Inventory/HollowInventoryComponent.h"
#include "Items/HollowItemDefinition.h"
#include "Profile/HollowProfileTypes.h"
#include "Tags/HollowGameplayTags.h"
#include "Tests/AutomationCommon.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace HollowUpgradeTests
{
	// Rules data only: the shipped upgrades are DA_Upgrade_* (create_economy_assets.py).
	const FPrimaryAssetId Welder(TEXT("HollowItem"), TEXT("Test_Welder"));
	const FPrimaryAssetId Cutter(TEXT("HollowItem"), TEXT("Test_Cutter"));
	const FPrimaryAssetId Charm(TEXT("HollowItem"), TEXT("Test_Charm"));
	const FPrimaryAssetId FuelCell(TEXT("HollowItem"), TEXT("Test_FuelCell"));
	const FPrimaryAssetId Core(TEXT("HollowItem"), TEXT("Test_Core"));
	const FPrimaryAssetId Scrap(TEXT("HollowItem"), TEXT("Test_Scrap"));
	const FPrimaryAssetId Reserve(TEXT("HollowUpgrade"), TEXT("Test_Reserve"));
	const FPrimaryAssetId Rider(TEXT("HollowUpgrade"), TEXT("Test_Rider"));
	const FPrimaryAssetId Bonus(TEXT("HollowUpgrade"), TEXT("Test_Bonus"));
	const FPrimaryAssetId Unknown(TEXT("HollowUpgrade"), TEXT("Test_Unknown"));
	constexpr int32 ReservePrice = 600;
	constexpr int32 RiderPrice = 800;
	constexpr int32 BonusPrice = 1000;
	constexpr int32 CoreValue = 500;
	constexpr int32 CoreBonusPercent = 20;
	constexpr int32 ReserveCells = 2;
	const TCHAR* const CellItemPath = TEXT("/Game/Hollow/Items/Consumables/DA_Item_FuelCell.DA_Item_FuelCell");

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

	FHollowUpgradeTerms Upgrade(const FPrimaryAssetId& Id, const int32 Price, const int32 Slots, const int32 CorePercent, TArray<FHollowItemStack> Items)
	{
		FHollowUpgradeTerms Result;
		Result.Id = Id;
		Result.Price = Price;
		Result.ExtraInsuredSlots = Slots;
		Result.CoreValuePercent = CorePercent;
		Result.RaidItems = MoveTemp(Items);
		return Result;
	}

	/** The S5-03 upgrades on test items, and enough of the S5-02 economy to insure. */
	FHollowEconomyTerms MakeTerms()
	{
		FHollowEconomyTerms Terms;
		Terms.Catalog.Add(Welder, 300, /*bInsurable=*/true, 1, HollowTags::Item_Type_Tool);
		Terms.Catalog.Add(Cutter, 400, /*bInsurable=*/true, 1, HollowTags::Item_Type_Tool);
		Terms.Catalog.Add(Charm, 25, /*bInsurable=*/true, 5, HollowTags::Item_Type_Tool);
		Terms.Catalog.Add(FuelCell, 25, /*bInsurable=*/false, 10, HollowTags::Item_Type_Consumable);
		Terms.Catalog.Add(Core, CoreValue, /*bInsurable=*/false, 1, HollowTags::Item_Type_AetherCore);
		Terms.Catalog.Add(Scrap, 8, /*bInsurable=*/false, 50, HollowTags::Item_Type_Material);
		Terms.Upgrades.Add(Reserve, Upgrade(Reserve, ReservePrice, 0, 0, {Stack(FuelCell, ReserveCells)}));
		Terms.Upgrades.Add(Rider, Upgrade(Rider, RiderPrice, 1, 0, {}));
		Terms.Upgrades.Add(Bonus, Upgrade(Bonus, BonusPrice, 0, CoreBonusPercent, {}));
		Terms.InsurancePercent = 30;
		Terms.MaxInsuredSlots = 2;
		return Terms;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowUpgradesBuyTest, "Hollow.Economy.Upgrades.Buy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowUpgradesBuyTest::RunTest(const FString& Parameters)
{
	// ADR-0032: a ship upgrade is bought once and for good, for its price, in the hub only; an unknown one is refused.
	using namespace HollowUpgradeTests;
	using R = EHollowTransactionResult;
	const FHollowEconomyTerms Terms = MakeTerms();
	FHollowProfileState State;
	State.Profile.Credits = ReservePrice + 100;
	TestTrue(TEXT("The fuel reserve"), HollowCraftingRules::Apply(State, FHollowProfileTransaction::BuyUpgrade(Reserve), Terms) == R::Applied
		&& State.Profile.Credits == 100 && State.Profile.Upgrades == TArray<FPrimaryAssetId>{Reserve});
	TestTrue(TEXT("...once"), HollowCraftingRules::Apply(State, FHollowProfileTransaction::BuyUpgrade(Reserve), Terms) == R::AlreadyOwned && State.Profile.Credits == 100);
	TestTrue(TEXT("Not enough credits for the rider"), HollowCraftingRules::Apply(State, FHollowProfileTransaction::BuyUpgrade(Rider), Terms) == R::NotEnoughCredits
		&& State.Profile.Upgrades.Num() == 1);
	TestTrue(TEXT("No such upgrade"), HollowCraftingRules::Apply(State, FHollowProfileTransaction::BuyUpgrade(Unknown), Terms) == R::UnknownUpgrade);
	State.Profile.Credits = BonusPrice;
	State.Escrow.RaidId = FGuid::NewGuid();
	TestTrue(TEXT("Not during a raid"), HollowCraftingRules::Apply(State, FHollowProfileTransaction::BuyUpgrade(Bonus), Terms) == R::InRaid
		&& State.Profile.Credits == BonusPrice);
	State.Escrow = FHollowRaidEscrow();
	TestTrue(TEXT("...in the hub"), HollowCraftingRules::Apply(State, FHollowProfileTransaction::BuyUpgrade(Bonus), Terms) == R::Applied && State.Profile.Credits == 0
		&& State.Profile.Upgrades.Num() == 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowUpgradesEffectsTest, "Hollow.Economy.Upgrades.Effects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowUpgradesEffectsTest::RunTest(const FString& Parameters)
{
	// What the upgrades do: the rider insures a third slot; the core contract pays 20 % more for cores (only cores); the
	// fuel reserve's cells come into the raid inventory free: held, used first, never spent from the profile.
	using namespace HollowUpgradeTests;
	using R = EHollowTransactionResult;
	const FHollowEconomyTerms Terms = MakeTerms();
	const FHollowUpgradeEffects All = HollowCraftingRules::SumEffects({Reserve, Rider, Bonus, Rider, Unknown}, Terms.Upgrades);
	TestTrue(TEXT("Effects add up, each upgrade once, unknown ones nothing"), All.ExtraInsuredSlots == 1 && All.CoreValuePercent == CoreBonusPercent
		&& All.RaidItems.Num() == 1 && All.RaidItems[0].Item == FuelCell && All.RaidItems[0].Count == ReserveCells);

	FHollowProfile Profile;
	Profile.Credits = 1000;
	Profile.Loadout = {Slot(Welder, 1, true), Slot(Cutter, 1, true), Slot(Charm, 1)};
	TestTrue(TEXT("Two insured slots: a third is over the limit"), HollowCraftingRules::Insure(Profile, 2, Terms) == R::InsuranceLimit);
	Profile.Upgrades = {Rider};
	TestTrue(TEXT("With the rider it is insured"), HollowCraftingRules::Insure(Profile, 2, Terms) == R::Applied && Profile.Loadout[2].bInsured);

	FHollowRaidContext Raid;
	Raid.Loot = {Stack(Core, 2), Stack(Scrap, 1)};
	Raid.CoreValuePercent = CoreBonusPercent;
	const FHollowProfileDelta Delta = FHollowExtractionResolver::Resolve(Raid, EHollowRaidOutcome::Extracted, Terms.Catalog);
	TestEqual(TEXT("Cores bring 20 % more, other loot its value"), Delta.Credits, 2 * CoreValue * (100 + CoreBonusPercent) / 100 + 8);
	const FDateTime Now(2026, 11, 22, 12, 0, 0);
	FHollowProfileState State;
	HollowProfileRules::BeginRaid(State, FGuid::NewGuid(), Now, Terms.Catalog);
	FHollowRaidResult Result;
	Result.RaidId = State.Escrow.RaidId;
	Result.Outcome = EHollowRaidOutcome::Extracted;
	Result.Loot = {Stack(Core, 1)};
	const TOptional<FHollowRaidReport> Report = HollowProfileRules::ApplyResult(State, Result, Terms.Catalog, CoreBonusPercent);
	TestTrue(TEXT("The host's result with the bonus: 600 for a core"), Report.IsSet() && Report->CreditsAfter == CoreValue * (100 + CoreBonusPercent) / 100);

	// The reserve's cells in a raid inventory (a player state's part: an ability system and the inventory).
	UHollowItemDefinition* CellItem = LoadObject<UHollowItemDefinition>(nullptr, CellItemPath);
	FTestWorldWrapper TestWorld;
	if (!TestNotNull(TEXT("DA_Item_FuelCell"), CellItem) || !TestWorld.CreateTestWorld(EWorldType::Game))
	{
		TestWorld.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = TestWorld.GetTestWorld();
	World->InitializeActorsForPlay(FURL());
	AActor* Owner = World->SpawnActor<AActor>();
	UAbilitySystemComponent* AbilitySystem = NewObject<UAbilitySystemComponent>(Owner);
	AbilitySystem->RegisterComponent();
	AbilitySystem->InitAbilityActorInfo(Owner, Owner);
	UHollowInventoryComponent* Inventory = NewObject<UHollowInventoryComponent>(Owner);
	Inventory->RegisterComponent();
	Inventory->ResetFromLoadout({Stack(CellItem->GetPrimaryAssetId(), 3)}, {Stack(CellItem->GetPrimaryAssetId(), ReserveCells)});
	TestEqual(TEXT("Three brought, two from the ship: five held"), Inventory->GetCount(CellItem), 3 + ReserveCells);
	Inventory->ConsumeItem(CellItem, ReserveCells);
	TestTrue(TEXT("The ship's two used first: nothing spent from the profile"), Inventory->GetSpent().IsEmpty());
	Inventory->ConsumeItem(CellItem, 1);
	TestTrue(TEXT("...the next one is the player's"), Inventory->GetSpent().Num() == 1 && Inventory->GetSpent()[0].Count == 1);
	Inventory->ResetFromLoadout({}, {Stack(CellItem->GetPrimaryAssetId(), ReserveCells)});
	TestTrue(TEXT("Only the ship's cells: held, and the refuel granted"), Inventory->GetCount(CellItem) == ReserveCells
		&& AbilitySystem->GetActivatableAbilities().Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowUpgradesDataTest, "Hollow.Economy.Upgrades.Data",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowUpgradesDataTest::RunTest(const FString& Parameters)
{
	// The shipped upgrades (S5-03 clarifications 1-3): the fuel reserve (600: two fuel cells a raid), the insurance rider
	// (800: one more insured slot), the core contract (1000: +20 % for cores); all valid, all in the economy's terms.
	TMap<FPrimaryAssetId, FHollowUpgradeTerms> Upgrades = UHollowShipUpgradeDefinition::LoadAllById();
	const FHollowUpgradeTerms* Reserve = Upgrades.Find(FPrimaryAssetId(TEXT("HollowUpgrade"), TEXT("DA_Upgrade_FuelReserve")));
	const FHollowUpgradeTerms* Rider = Upgrades.Find(FPrimaryAssetId(TEXT("HollowUpgrade"), TEXT("DA_Upgrade_InsuranceSlot")));
	const FHollowUpgradeTerms* Bonus = Upgrades.Find(FPrimaryAssetId(TEXT("HollowUpgrade"), TEXT("DA_Upgrade_CoreBonus")));
	if (!TestTrue(TEXT("Three upgrades"), Upgrades.Num() == 3 && Reserve && Rider && Bonus))
	{
		return false;
	}
	TestTrue(TEXT("Fuel reserve: 600, two fuel cells"), Reserve->Price == 600 && Reserve->RaidItems.Num() == 1 && Reserve->RaidItems[0].Count == 2
		&& Reserve->RaidItems[0].Item == FPrimaryAssetId(TEXT("HollowItem"), TEXT("DA_Item_FuelCell")));
	TestTrue(TEXT("Insurance rider: 800, one slot"), Rider->Price == 800 && Rider->ExtraInsuredSlots == 1);
	TestTrue(TEXT("Core contract: 1000, +20 %"), Bonus->Price == 1000 && Bonus->CoreValuePercent == 20);
	const FHollowEconomyTerms Terms = FHollowEconomyTerms::Load(FHollowProfileState());
	TestEqual(TEXT("The economy's terms know them"), Terms.Upgrades.Num(), 3);
	TestTrue(TEXT("...and the reserve's item"), Terms.Catalog.GetItemType(Reserve->RaidItems[0].Item).MatchesTag(HollowTags::Item_Type_Consumable));
#if WITH_EDITOR   // Data Validation exists in editor builds only
	for (const TPair<FPrimaryAssetId, FHollowUpgradeTerms>& Pair : Upgrades)
	{
		const UHollowShipUpgradeDefinition* Asset = Cast<UHollowShipUpgradeDefinition>(UAssetManager::Get().GetPrimaryAssetPath(Pair.Key).TryLoad());
		FDataValidationContext Validation;
		TestTrue(Pair.Key.ToString() + TEXT(" is valid"), Asset && Asset->IsDataValid(Validation) != EDataValidationResult::Invalid && Validation.GetNumErrors() == 0);
	}
#endif
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
