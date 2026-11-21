#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/GameplayAbility.h"
#include "Abilities/HollowAbilitySet.h"
#include "Abilities/HollowGameplayAbility.h"
#include "Abilities/HollowItemCostSource.h"
#include "AbilitySystemComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "Extraction/HollowExtractionResolver.h"
#include "Inventory/HollowInventoryComponent.h"
#include "Inventory/HollowLoadoutRules.h"
#include "Items/HollowItemDefinition.h"
#include "Profile/HollowEconomySettings.h"
#include "Profile/HollowProfileTypes.h"
#include "Tags/HollowGameplayTags.h"
#include "Tests/AutomationCommon.h"

namespace HollowLoadoutTests
{
	// Rules data only: the real items are DA_Item_* (create_raid_flow_assets.py, create_inventory_assets.py).
	const FPrimaryAssetId Welder(TEXT("HollowItem"), TEXT("Test_Welder"));
	const FPrimaryAssetId Cutter(TEXT("HollowItem"), TEXT("Test_Cutter"));
	const FPrimaryAssetId FuelCell(TEXT("HollowItem"), TEXT("Test_FuelCell"));
	const FPrimaryAssetId Mine(TEXT("HollowItem"), TEXT("Test_Mine"));
	const FPrimaryAssetId Core(TEXT("HollowItem"), TEXT("Test_Core"));
	const FPrimaryAssetId Unknown(TEXT("HollowItem"), TEXT("Test_Unknown"));
	constexpr int32 CellStack = 10;
	constexpr int32 MineStack = 3;
	constexpr int32 MaxSlots = 4;

	// The real items and what they grant.
	const TCHAR* const WelderItemPath = TEXT("/Game/Hollow/Items/Tools/DA_Item_Welder.DA_Item_Welder");
	const TCHAR* const MineItemPath = TEXT("/Game/Hollow/Items/Consumables/DA_Item_PulseMine.DA_Item_PulseMine");
	const TCHAR* const CellItemPath = TEXT("/Game/Hollow/Items/Consumables/DA_Item_FuelCell.DA_Item_FuelCell");
	const TCHAR* const CoreItemPath = TEXT("/Game/Hollow/Items/AetherCore/DA_Item_AetherCore.DA_Item_AetherCore");
	const TCHAR* const WelderAbilityClass = TEXT("/Game/Hollow/Tools/Welder/GA_Tool_Welder.GA_Tool_Welder_C");
	const TCHAR* const MineAbilityClass = TEXT("/Game/Hollow/Gadgets/PulseMine/GA_PulseMine.GA_PulseMine_C");
	const TCHAR* const RefuelAbilityClass = TEXT("/Game/Hollow/Tools/Refuel/GA_ToolRefuel.GA_ToolRefuel_C");

	FHollowItemCatalog MakeCatalog()
	{
		FHollowItemCatalog Catalog;
		Catalog.Add(Welder, 300, /*bInsurable=*/true, 1, HollowTags::Item_Type_Tool);
		Catalog.Add(Cutter, 400, /*bInsurable=*/true, 1, HollowTags::Item_Type_Tool);
		Catalog.Add(FuelCell, 25, /*bInsurable=*/false, CellStack, HollowTags::Item_Type_Consumable);
		Catalog.Add(Mine, 40, /*bInsurable=*/false, MineStack, HollowTags::Item_Type_Consumable);
		Catalog.Add(Core, 500, /*bInsurable=*/false, 1, HollowTags::Item_Type_AetherCore);
		return Catalog;
	}

	FGameplayTagContainer RaidTypes()
	{
		FGameplayTagContainer Types;
		Types.AddTag(HollowTags::Item_Type_Tool);
		Types.AddTag(HollowTags::Item_Type_Consumable);
		return Types;
	}

	FHollowItemStack Stack(const FPrimaryAssetId& Item, const int32 Count)
	{
		FHollowItemStack Result;
		Result.Item = Item;
		Result.Count = Count;
		return Result;
	}

	FHollowLoadoutSlot Slot(const FPrimaryAssetId& Item, const int32 Count, const bool bInsured)
	{
		FHollowLoadoutSlot Result;
		Result.Item = Item;
		Result.Count = Count;
		Result.bInsured = bInsured;
		return Result;
	}

	int32 CountOf(const TArray<FHollowItemStack>& Stacks, const FPrimaryAssetId& Item)
	{
		const FHollowItemStack* Found = Stacks.FindByPredicate([&Item](const FHollowItemStack& S) { return S.Item == Item; });
		return Found ? Found->Count : 0;
	}

	bool HasIssue(const TArray<FHollowLoadoutIssue>& Issues, const FPrimaryAssetId& Item, const EHollowLoadoutIssue Issue)
	{
		return Issues.Contains(FHollowLoadoutIssue{Item, Issue});
	}

	/** Welder insured, cutter not, three fuel cells and two mines (neither can be insured). */
	TArray<FHollowLoadoutSlot> MakeLoadout()
	{
		return {Slot(Welder, 1, true), Slot(Cutter, 1, false), Slot(FuelCell, 3, false), Slot(Mine, 2, false)};
	}

	const FGameplayAbilitySpec* FindSpec(const UAbilitySystemComponent& AbilitySystem, const UClass* AbilityClass)
	{
		for (const FGameplayAbilitySpec& Spec : AbilitySystem.GetActivatableAbilities())
		{
			if (Spec.Ability && Spec.Ability->GetClass() == AbilityClass)
			{
				return &Spec;
			}
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowLoadoutSyncValidateTest, "Hollow.Economy.LoadoutSync.Validate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowLoadoutSyncValidateTest::RunTest(const FString& Parameters)
{
	// ADR-0030: what a raid takes of a loadout. Soft: refused slots are dropped (unknown, not a raid type, no count), a
	// slot over its stack is cut down, slots past the limit are dropped; accepted slots of one item merge.
	using namespace HollowLoadoutTests;
	const TArray<FHollowItemStack> Slots = {Stack(Welder, 1), Stack(Core, 1), Stack(Unknown, 1), Stack(Mine, MineStack + 2), Stack(FuelCell, 0),
		Stack(FuelCell, 4), Stack(Mine, 2), Stack(Cutter, 1)};
	TArray<FHollowLoadoutIssue> Issues;
	const TArray<FHollowItemStack> Accepted = HollowLoadoutRules::Validate(Slots, MakeCatalog(), RaidTypes(), MaxSlots, &Issues);
	TestEqual(TEXT("One stack per item, in first-slot order"), Accepted.Num(), 3);
	TestTrue(TEXT("...welder, mines, fuel cells"), Accepted.Num() == 3 && Accepted[0].Item == Welder && Accepted[1].Item == Mine && Accepted[2].Item == FuelCell);
	TestEqual(TEXT("The welder"), CountOf(Accepted, Welder), 1);
	TestEqual(TEXT("Mines: a slot cut down to its stack, merged with the next"), CountOf(Accepted, Mine), MineStack + 2);
	TestEqual(TEXT("Fuel cells: the empty slot dropped"), CountOf(Accepted, FuelCell), 4);
	TestEqual(TEXT("The cutter came after the most slots a raid takes"), CountOf(Accepted, Cutter), 0);
	TestTrue(TEXT("An Aether core is not taken into a raid"), CountOf(Accepted, Core) == 0 && HasIssue(Issues, Core, EHollowLoadoutIssue::NotForRaids));
	TestTrue(TEXT("Unknown items are dropped"), CountOf(Accepted, Unknown) == 0 && HasIssue(Issues, Unknown, EHollowLoadoutIssue::UnknownItem));
	TestTrue(TEXT("Every issue is reported"), HasIssue(Issues, Mine, EHollowLoadoutIssue::OverStack) && HasIssue(Issues, FuelCell, EHollowLoadoutIssue::NoCount)
		&& HasIssue(Issues, Cutter, EHollowLoadoutIssue::TooManySlots) && Issues.Num() == 5);
	TestTrue(TEXT("Nothing in, nothing out"), HollowLoadoutRules::Validate({}, MakeCatalog(), RaidTypes(), MaxSlots).IsEmpty());

	const TArray<FHollowItemStack> FromProfile = HollowLoadoutRules::ToStacks(MakeLoadout());
	TestTrue(TEXT("A profile's loadout as stacks, slot by slot"), FromProfile.Num() == 4 && CountOf(FromProfile, FuelCell) == 3 && CountOf(FromProfile, Mine) == 2);
	TestTrue(TEXT("A valid loadout passes untouched"), HollowLoadoutRules::Validate(FromProfile, MakeCatalog(), RaidTypes(), MaxSlots) == FromProfile);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowLoadoutSyncResolveTest, "Hollow.Economy.LoadoutSync.Resolve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowLoadoutSyncResolveTest::RunTest(const FString& Parameters)
{
	// ADR-0030: what was used up in the raid is gone whatever the outcome (never more than the loadout held); the
	// outcome decides about the rest. Without the host's result (SignalLost) nothing is known to be spent.
	using namespace HollowLoadoutTests;
	const FHollowItemCatalog Catalog = MakeCatalog();
	FHollowRaidContext Context;
	Context.Loadout = MakeLoadout();
	Context.Spent = {Stack(FuelCell, 2), Stack(Mine, MineStack), Stack(Core, 1)};   // more mines than taken, a core never taken

	const FHollowProfileDelta Extracted = FHollowExtractionResolver::Resolve(Context, EHollowRaidOutcome::Extracted, Catalog);
	TestTrue(TEXT("Extracted: what was spent is gone, never more than held"), CountOf(Extracted.Spent, FuelCell) == 2 && CountOf(Extracted.Spent, Mine) == 2
		&& CountOf(Extracted.Spent, Core) == 0);
	TestTrue(TEXT("...the rest is kept"), CountOf(Extracted.Kept, Welder) == 1 && CountOf(Extracted.Kept, Cutter) == 1 && CountOf(Extracted.Kept, FuelCell) == 1
		&& CountOf(Extracted.Kept, Mine) == 0 && Extracted.Lost.IsEmpty());
	FHollowProfile Profile;
	Profile.Loadout = MakeLoadout();
	HollowProfileRules::Apply(Profile, Extracted, EHollowRaidOutcome::Extracted);
	TestTrue(TEXT("...the profile keeps one fuel cell, the mines' slot is gone"), Profile.Loadout.Num() == 3 && Profile.Loadout[2] == Slot(FuelCell, 1, false));

	const FHollowProfileDelta Dead = FHollowExtractionResolver::Resolve(Context, EHollowRaidOutcome::Dead, Catalog);
	TestTrue(TEXT("Dead: spent the same"), CountOf(Dead.Spent, FuelCell) == 2 && CountOf(Dead.Spent, Mine) == 2);
	TestTrue(TEXT("...only the insured welder kept, the rest lost"), Dead.Kept.Num() == 1 && CountOf(Dead.Kept, Welder) == 1 && CountOf(Dead.Lost, Cutter) == 1
		&& CountOf(Dead.Lost, FuelCell) == 1 && CountOf(Dead.Lost, Mine) == 0);
	Profile.Loadout = MakeLoadout();
	HollowProfileRules::Apply(Profile, Dead, EHollowRaidOutcome::Dead);
	TestTrue(TEXT("...the profile keeps the welder"), Profile.Loadout.Num() == 1 && Profile.Loadout[0] == Slot(Welder, 1, true));

	// The host's result carries the spending through the escrow; a lost host spends nothing.
	const FDateTime Now(2026, 11, 21, 12, 0, 0);
	const FGuid Raid = FGuid::NewGuid();
	FHollowProfileState State;
	State.Profile.Loadout = MakeLoadout();
	HollowProfileRules::BeginRaid(State, Raid, Now, Catalog);
	FHollowRaidResult Result;
	Result.RaidId = Raid;
	Result.Outcome = EHollowRaidOutcome::Extracted;
	Result.Spent = {Stack(Mine, 1)};
	TestTrue(TEXT("The catalog the rules need covers what was spent"), HollowProfileRules::ItemsToCatalog(State, &Result).Contains(Mine));
	const TOptional<FHollowRaidReport> Report = HollowProfileRules::ApplyResult(State, Result, Catalog);
	TestTrue(TEXT("The host's result: one mine spent, one kept"), Report.IsSet() && CountOf(Report->Delta.Spent, Mine) == 1 && CountOf(Report->Delta.Kept, Mine) == 1
		&& State.Profile.Loadout.Num() == 4 && State.Profile.Loadout[3] == Slot(Mine, 1, false));
	HollowProfileRules::BeginRaid(State, FGuid::NewGuid(), Now, Catalog);
	const TOptional<FHollowRaidReport> Lost = HollowProfileRules::ResolveEscrow(State, EHollowRaidOutcome::SignalLost, Now, Catalog);
	TestTrue(TEXT("Signal lost: nothing known spent, the loadout kept"), Lost.IsSet() && Lost->Delta.Spent.IsEmpty() && State.Profile.Loadout.Num() == 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowLoadoutSyncInventoryTest, "Hollow.Economy.LoadoutSync.Inventory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowLoadoutSyncInventoryTest::RunTest(const FString& Parameters)
{
	// ADR-0030: the raid inventory with the real items. The accepted loadout becomes its entries, each item grants its
	// ability set with itself as the source object (unknown items are left out, the Aether core grants nothing); an
	// ability's item cost is taken from it, never below zero; spent = taken minus held; clearing takes the abilities
	// back. The owner hears every change, a received one included.
	using namespace HollowLoadoutTests;
	UHollowItemDefinition* WelderItem = LoadObject<UHollowItemDefinition>(nullptr, WelderItemPath);
	UHollowItemDefinition* MineItem = LoadObject<UHollowItemDefinition>(nullptr, MineItemPath);
	UHollowItemDefinition* CellItem = LoadObject<UHollowItemDefinition>(nullptr, CellItemPath);
	UHollowItemDefinition* CoreItem = LoadObject<UHollowItemDefinition>(nullptr, CoreItemPath);
	const UClass* WelderAbility = LoadClass<UGameplayAbility>(nullptr, WelderAbilityClass);
	const UClass* MineAbility = LoadClass<UGameplayAbility>(nullptr, MineAbilityClass);
	const UClass* RefuelAbility = LoadClass<UGameplayAbility>(nullptr, RefuelAbilityClass);
	FTestWorldWrapper TestWorld;
	if (!TestNotNull(TEXT("DA_Item_Welder"), WelderItem) || !TestNotNull(TEXT("DA_Item_PulseMine"), MineItem) || !TestNotNull(TEXT("DA_Item_FuelCell"), CellItem)
		|| !TestNotNull(TEXT("DA_Item_AetherCore"), CoreItem) || !TestNotNull(TEXT("GA_Tool_Welder"), WelderAbility) || !TestNotNull(TEXT("GA_PulseMine"), MineAbility)
		|| !TestNotNull(TEXT("GA_ToolRefuel"), RefuelAbility) || !TestWorld.CreateTestWorld(EWorldType::Game))
	{
		TestWorld.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = TestWorld.GetTestWorld();
	World->InitializeActorsForPlay(FURL());
	// A player state's part: an ability system and the inventory on one actor the server owns.
	AActor* Owner = World->SpawnActor<AActor>();
	UAbilitySystemComponent* AbilitySystem = NewObject<UAbilitySystemComponent>(Owner);
	AbilitySystem->RegisterComponent();
	AbilitySystem->InitAbilityActorInfo(Owner, Owner);
	UHollowInventoryComponent* Inventory = NewObject<UHollowInventoryComponent>(Owner);
	Inventory->RegisterComponent();
	int32 Changes = 0;
	Inventory->OnInventoryChanged.AddLambda([&Changes] { ++Changes; });

	AddExpectedMessage(TEXT("left out of the inventory"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1, /*IsRegex=*/false);
	Inventory->ResetFromLoadout({Stack(WelderItem->GetPrimaryAssetId(), 1), Stack(MineItem->GetPrimaryAssetId(), 2), Stack(CellItem->GetPrimaryAssetId(), 3),
		Stack(Unknown, 1)});
	TestEqual(TEXT("Three entries, the unknown item left out"), Inventory->GetEntries().Num(), 3);
	TestTrue(TEXT("Counts by definition and by id"), Inventory->GetCount(MineItem) == 2 && Inventory->GetCount(CellItem->GetPrimaryAssetId()) == 3);
	TestTrue(TEXT("Every change was heard"), Changes >= 3);
	const FGameplayAbilitySpec* Welding = FindSpec(*AbilitySystem, WelderAbility);
	const FGameplayAbilitySpec* Mining = FindSpec(*AbilitySystem, MineAbility);
	const FGameplayAbilitySpec* Refuelling = FindSpec(*AbilitySystem, RefuelAbility);
	TestTrue(TEXT("The welder on its input, the item its source"), Welding && Welding->SourceObject.Get() == WelderItem
		&& Welding->GetDynamicSpecSourceTags().HasTagExact(HollowTags::Input_SecondaryUse));
	TestTrue(TEXT("The mine on the gadget input"), Mining && Mining->SourceObject.Get() == MineItem && Mining->GetDynamicSpecSourceTags().HasTagExact(HollowTags::Input_Gadget));
	TestTrue(TEXT("The refuel on the reload input"), Refuelling && Refuelling->SourceObject.Get() == CellItem
		&& Refuelling->GetDynamicSpecSourceTags().HasTagExact(HollowTags::Input_Reload));
	const int32 Granted = AbilitySystem->GetActivatableAbilities().Num();
	Inventory->AddItem(CoreItem, 1);
	TestEqual(TEXT("The Aether core grants nothing"), AbilitySystem->GetActivatableAbilities().Num(), Granted);
	Inventory->AddItem(WelderItem, 1);
	TestTrue(TEXT("More of an item held grants nothing twice"), AbilitySystem->GetActivatableAbilities().Num() == Granted && Inventory->GetCount(WelderItem) == 2);

	IHollowItemCostSource* Source = IHollowItemCostSource::Find(Owner);
	TestTrue(TEXT("Abilities find the inventory on their owner"), Source == Inventory);
	const int32 ChangesBefore = Changes;
	TestTrue(TEXT("A mine is taken"), Source && Source->ConsumeItem(MineItem, 1) && Inventory->GetCount(MineItem) == 1 && Changes == ChangesBefore + 1);
	TestFalse(TEXT("...never more than held"), Source && Source->ConsumeItem(MineItem, 2));
	TestTrue(TEXT("...nothing taken then"), Source && Source->GetItemCount(MineItem) == 1);
	TestTrue(TEXT("...nor of what it does not hold"), !Inventory->ConsumeItem(GetMutableDefault<UHollowItemDefinition>(), 1));
	Inventory->ConsumeItem(MineItem, 1);
	TestTrue(TEXT("Used up: the entry stays at 0, its ability too"), Inventory->GetCount(MineItem) == 0 && Inventory->GetEntries().Num() == 4
		&& FindSpec(*AbilitySystem, MineAbility) != nullptr);
	const TArray<FHollowItemStack> Spent = Inventory->GetSpent();
	TestTrue(TEXT("Spent: the two mines only (added items count as taken)"), Spent.Num() == 1 && CountOf(Spent, MineItem->GetPrimaryAssetId()) == 2);

	const int32 BeforeReceive = Changes;
	PRAGMA_DISABLE_DEPRECATION_WARNINGS   // the parameters still carry a deprecated member
	const FFastArraySerializer::FPostReplicatedReceiveParameters Received{};
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	Inventory->Inventory.PostReplicatedReceive(Received);
	TestEqual(TEXT("On the owner's machine a received change is heard"), Changes, BeforeReceive + 1);

	Inventory->Clear();
	TestTrue(TEXT("Cleared: empty, the abilities taken back"), Inventory->GetEntries().IsEmpty() && !FindSpec(*AbilitySystem, WelderAbility)
		&& !FindSpec(*AbilitySystem, MineAbility) && !FindSpec(*AbilitySystem, RefuelAbility));
	Inventory->ResetFromLoadout({Stack(CellItem->GetPrimaryAssetId(), 1)});
	TestTrue(TEXT("A new loadout replaces it"), Inventory->GetEntries().Num() == 1 && FindSpec(*AbilitySystem, RefuelAbility) && !FindSpec(*AbilitySystem, WelderAbility));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowLoadoutSyncDataTest, "Hollow.Economy.LoadoutSync.Data",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowLoadoutSyncDataTest::RunTest(const FString& Parameters)
{
	// The shipped data: a new profile's loadout is a valid raid loadout taken as it is, and every item in it does
	// something in a raid (its ability set loads and grants an ability).
	using namespace HollowLoadoutTests;
	const UHollowEconomySettings* Settings = GetDefault<UHollowEconomySettings>();
	TestTrue(TEXT("Loadout limits are set"), Settings->MaxLoadoutSlots >= Settings->StartingLoadout.Num() && Settings->LoadoutItemTypes.HasTagExact(HollowTags::Item_Type_Tool)
		&& Settings->LoadoutItemTypes.HasTagExact(HollowTags::Item_Type_Consumable) && !Settings->LoadoutItemTypes.HasTag(HollowTags::Item_Type_AetherCore));
	const TArray<FHollowItemStack> Starting = HollowLoadoutRules::ToStacks(Settings->StartingLoadout);
	TArray<FHollowLoadoutIssue> Issues;
	const TArray<FHollowItemStack> Accepted = HollowLoadoutRules::ValidateForRaid(Starting, &Issues);
	TestTrue(FString::Printf(TEXT("The starting loadout goes in as it is (%s)"), *HollowLoadoutRules::Describe(Issues)), Issues.IsEmpty() && Accepted == Starting);
	TestTrue(TEXT("...pulse mines among it"), CountOf(Accepted, FPrimaryAssetId(TEXT("HollowItem"), TEXT("DA_Item_PulseMine"))) > 0);
	for (const FHollowItemStack& Item : Starting)
	{
		const UHollowItemDefinition* Definition = Cast<UHollowItemDefinition>(UAssetManager::Get().GetPrimaryAssetPath(Item.Item).TryLoad());
		const UHollowAbilitySet* AbilitySet = Definition ? Definition->AbilitySet.LoadSynchronous() : nullptr;
		TestTrue(Item.Item.ToString() + TEXT(" grants an ability in a raid"), AbilitySet && !AbilitySet->GrantedAbilities.IsEmpty() && AbilitySet->GrantedAbilities[0].Ability);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
