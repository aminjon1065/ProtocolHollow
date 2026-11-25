#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Crafting/HollowCraftingRules.h"
#include "Items/HollowItemTypes.h"
#include "Profile/HollowEconomySettings.h"
#include "Tags/HollowGameplayTags.h"

namespace HollowWeaponRuleTests
{
	// Hand-made catalog: invented ids, never loaded.
	const FPrimaryAssetId Nailgun(TEXT("HollowItem"), TEXT("Test_Nailgun"));
	const FPrimaryAssetId Shotgun(TEXT("HollowItem"), TEXT("Test_Shotgun"));
	const FPrimaryAssetId Nails(TEXT("HollowItem"), TEXT("Test_Nails"));
	const FPrimaryAssetId Cutter(TEXT("HollowItem"), TEXT("Test_Cutter"));
	constexpr int32 NailsStack = 60;

	FHollowItemCatalog Catalog()
	{
		FHollowItemCatalog Result;
		Result.Add(Nailgun, 150, true, 1, HollowTags::Item_Type_Weapon, HollowTags::Item_Slot_Weapon);
		Result.Add(Shotgun, 650, true, 1, HollowTags::Item_Type_Weapon, HollowTags::Item_Slot_Weapon);
		Result.Add(Nails, 1, false, NailsStack, HollowTags::Item_Type_Ammo);
		Result.Add(Cutter, 400, true, 1, HollowTags::Item_Type_Tool, HollowTags::Item_Slot_Primary);
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowWeaponRuleTest, "Hollow.Economy.Weapons.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowWeaponRuleTest::RunTest(const FString& Parameters)
{
	// GP-01 (ADR-0044): one weapon per loadout, its rounds without a slot; the shipped config says so.
	using namespace HollowWeaponRuleTests;
	using HollowCraftingRules::CanTakeIntoSlot;
	const FHollowItemCatalog Items = Catalog();
	const TMap<FGameplayTag, int32> Limits = {{HollowTags::Item_Slot_Weapon, 1}, {HollowTags::Item_Slot_Primary, 1}};
	TestTrue(TEXT("An empty loadout takes a weapon"), CanTakeIntoSlot({}, Shotgun, Items, Limits));
	TestFalse(TEXT("A second weapon is refused"), CanTakeIntoSlot({Nailgun}, Shotgun, Items, Limits));
	TestFalse(TEXT("...and the same weapon twice"), CanTakeIntoSlot({Nailgun}, Nailgun, Items, Limits));
	TestTrue(TEXT("Its rounds have no slot"), CanTakeIntoSlot({Nailgun, Nails}, Nails, Items, Limits));
	TestTrue(TEXT("The weapon takes no tool's slot"), CanTakeIntoSlot({Nailgun}, Cutter, Items, Limits));
	TestEqual(TEXT("The rounds stack"), Items.GetMaxStack(Nails), NailsStack);

	const UHollowEconomySettings* Settings = GetDefault<UHollowEconomySettings>();
	const FHollowEquipSlotLimit* Weapon = Settings->EquipSlotLimits.FindByPredicate([](const FHollowEquipSlotLimit& Limit)
	{
		return Limit.Slot == HollowTags::Item_Slot_Weapon;
	});
	TestTrue(TEXT("DefaultGame.ini: one weapon per loadout"), Weapon && Weapon->Limit == 1);
	TestTrue(TEXT("DefaultGame.ini: weapons and rounds go into a raid"), Settings->LoadoutItemTypes.HasTagExact(HollowTags::Item_Type_Weapon)
		&& Settings->LoadoutItemTypes.HasTagExact(HollowTags::Item_Type_Ammo));
	TestTrue(TEXT("DefaultGame.ini: room for a weapon and its rounds next to the tools, gadget and relic"), Settings->MaxLoadoutSlots >= 8);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
