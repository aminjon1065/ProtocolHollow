#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/HollowAbilitySet.h"
#include "Classes/HollowClassDefinition.h"
#include "Crafting/HollowCraftingRules.h"
#include "GameplayEffect.h"
#include "Profile/HollowEconomySettings.h"
#include "Profile/HollowProfileTypes.h"
#include "Tags/HollowGameplayTags.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace HollowClassTests
{
	using R = EHollowTransactionResult;

	// Hand-made terms: invented ids, never loaded.
	const FPrimaryAssetId Welder(TEXT("HollowItem"), TEXT("Test_Welder"));
	const FPrimaryAssetId Cell(TEXT("HollowItem"), TEXT("Test_Cell"));
	const FPrimaryAssetId Breacher(TEXT("HollowClass"), TEXT("Test_Breacher"));
	const FPrimaryAssetId Seer(TEXT("HollowClass"), TEXT("Test_Seer"));
	const FPrimaryAssetId Ghost(TEXT("HollowClass"), TEXT("Test_Ghost"));
	constexpr int32 BreacherCells = 3;
	constexpr int32 SeerCells = 2;
	constexpr int32 StashCells = 5;
	constexpr int32 Credits = 100;

	FHollowLoadoutSlot Slot(const FPrimaryAssetId& Item, const int32 Count, const bool bInsured = false)
	{
		FHollowLoadoutSlot Result;
		Result.Item = Item;
		Result.Count = Count;
		Result.bInsured = bInsured;
		return Result;
	}

	FHollowEconomyTerms MakeTerms()
	{
		FHollowEconomyTerms Terms;
		FHollowClassTerms First;
		First.Id = Breacher;
		First.StartingLoadout = {Slot(Welder, 1, true), Slot(Cell, BreacherCells)};
		Terms.Classes.Add(Breacher, First);
		FHollowClassTerms Second;
		Second.Id = Seer;
		Second.StartingLoadout = {Slot(Cell, SeerCells)};
		Terms.Classes.Add(Seer, Second);
		Terms.DefaultClass = Breacher;
		return Terms;
	}

	const FHollowClassLoadout* Kept(const FHollowProfile& Profile, const FPrimaryAssetId& Class)
	{
		return Profile.ClassLoadouts.FindByPredicate([&Class](const FHollowClassLoadout& Each) { return Each.ClassId == Class; });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowClassSelectTest, "Hollow.Economy.Class.Select",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowClassSelectTest::RunTest(const FString& Parameters)
{
	// ADR-0034: each class keeps its own loadout, a class's starting loadout comes once, the stash is shared.
	using namespace HollowClassTests;
	const FHollowEconomyTerms Terms = MakeTerms();
	FHollowProfileState State;
	State.Profile.Credits = Credits;
	State.Profile.ClassId = Breacher;
	State.Profile.Loadout = {Slot(Welder, 1, true), Slot(Cell, 1)};   // the breacher's, partly used
	State.Profile.Stash = {FHollowItemStack()};
	State.Profile.Stash[0].Item = Cell;
	State.Profile.Stash[0].Count = StashCells;
	const TArray<FHollowLoadoutSlot> BreacherLoadout = State.Profile.Loadout;
	const TArray<FHollowItemStack> Stash = State.Profile.Stash;

	// First pick: the seer's starting loadout; the breacher's is kept for it.
	const FHollowProfileTransaction ToSeer = FHollowProfileTransaction::SelectClass(Seer);
	TestEqual(TEXT("The seer is picked"), HollowCraftingRules::Apply(State, ToSeer, Terms), R::Applied);
	TestTrue(TEXT("...with its starting loadout"), State.Profile.ClassId == Seer && State.Profile.Loadout == Terms.Classes[Seer].StartingLoadout);
	const FHollowClassLoadout* Breachers = Kept(State.Profile, Breacher);
	TestTrue(TEXT("...the breacher's loadout kept as it was"), Breachers && Breachers->Loadout == BreacherLoadout && State.Profile.ClassLoadouts.Num() == 1);
	TestTrue(TEXT("...nothing bought, the stash shared"), State.Profile.Credits == Credits && State.Profile.Stash == Stash);
	TestEqual(TEXT("The same transaction again changes nothing"), HollowCraftingRules::Apply(State, ToSeer, Terms), R::AlreadyApplied);
	TestEqual(TEXT("The class played already"), HollowCraftingRules::Apply(State, FHollowProfileTransaction::SelectClass(Seer), Terms), R::AlreadySelected);

	// Back and forth: every class gets its own loadout back, never its starting loadout again.
	State.Profile.Loadout[0].Count = 1;   // a cell used in a raid
	const TArray<FHollowLoadoutSlot> SeerLoadout = State.Profile.Loadout;
	TestEqual(TEXT("Back to the breacher"), HollowCraftingRules::Apply(State, FHollowProfileTransaction::SelectClass(Breacher), Terms), R::Applied);
	TestTrue(TEXT("...its own loadout back (not its kit)"), State.Profile.Loadout == BreacherLoadout && State.Profile.ClassLoadouts.Num() == 1
		&& Kept(State.Profile, Seer) && Kept(State.Profile, Seer)->Loadout == SeerLoadout);
	TestEqual(TEXT("The seer again"), HollowCraftingRules::Apply(State, FHollowProfileTransaction::SelectClass(Seer), Terms), R::Applied);
	TestTrue(TEXT("...its used loadout, no second kit"), State.Profile.Loadout == SeerLoadout);

	// Refusals change nothing.
	const FHollowProfileState Before = State;
	TestEqual(TEXT("Not a class"), HollowCraftingRules::Apply(State, FHollowProfileTransaction::SelectClass(Ghost), Terms), R::UnknownClass);
	FHollowProfileState InRaid = State;
	InRaid.Escrow.RaidId = FGuid::NewGuid();
	TestEqual(TEXT("A raid under way"), HollowCraftingRules::Apply(InRaid, FHollowProfileTransaction::SelectClass(Breacher), Terms), R::InRaid);
	TestTrue(TEXT("...the profile as it was"), State.Profile.ClassId == Before.Profile.ClassId && State.Profile.Loadout == Before.Profile.Loadout
		&& InRaid.Profile.Loadout == Before.Profile.Loadout);

	// A profile from before classes plays the default class with the loadout it has.
	FHollowProfileState Old;
	Old.Profile.Loadout = {Slot(Welder, 1)};
	TestTrue(TEXT("An old profile plays the default class"), HollowCraftingRules::CurrentClass(Old.Profile, Terms) == Breacher);
	TestEqual(TEXT("...which it cannot pick again"), HollowCraftingRules::Apply(Old, FHollowProfileTransaction::SelectClass(Breacher), Terms), R::AlreadySelected);
	TestEqual(TEXT("...and leaves for another"), HollowCraftingRules::Apply(Old, FHollowProfileTransaction::SelectClass(Seer), Terms), R::Applied);
	TestTrue(TEXT("...keeping its loadout for the default class"), Kept(Old.Profile, Breacher) && Kept(Old.Profile, Breacher)->Loadout.Num() == 1);
	FHollowEconomyTerms NoDefault = MakeTerms();
	NoDefault.DefaultClass = FPrimaryAssetId();
	FHollowProfileState Classless;
	Classless.Profile.Loadout = {Slot(Welder, 1)};
	TestEqual(TEXT("No class and no default: no loadout is given up"), HollowCraftingRules::Apply(Classless, FHollowProfileTransaction::SelectClass(Seer), NoDefault),
		R::BadRequest);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowClassDataTest, "Hollow.Economy.Class.Data",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowClassDataTest::RunTest(const FString& Parameters)
{
	// The shipped classes (create_class_assets.py): valid, the default one set, each with its own tag, effect and colour.
	const UHollowEconomySettings* Settings = GetDefault<UHollowEconomySettings>();
	const UHollowClassDefinition* Engineer = UHollowClassDefinition::Load(FPrimaryAssetId(UHollowClassDefinition::AssetType, TEXT("DA_Class_Engineer")));
	const UHollowClassDefinition* Occultist = UHollowClassDefinition::Load(FPrimaryAssetId(UHollowClassDefinition::AssetType, TEXT("DA_Class_Occultist")));
	if (!TestNotNull(TEXT("DA_Class_Engineer"), Engineer) || !TestNotNull(TEXT("DA_Class_Occultist"), Occultist))
	{
		return false;
	}
	TestTrue(TEXT("The Engineer is the default class"), Settings->DefaultClass == Engineer->GetPrimaryAssetId());
	TestEqual(TEXT("Two classes"), UHollowClassDefinition::LoadAll().Num(), 2);
	TestTrue(TEXT("Their tags"), Engineer->ClassTag == HollowTags::Class_Engineer && Occultist->ClassTag == HollowTags::Class_Occultist);
	TestFalse(TEXT("Their own visor colours"), Engineer->VisorColor.Equals(Occultist->VisorColor));
	const FHollowEconomyTerms Terms = FHollowEconomyTerms::Load(FHollowProfileState());
	for (const UHollowClassDefinition* Class : {Engineer, Occultist})
	{
		const FString Name = Class->GetName();
#if WITH_EDITOR
		FDataValidationContext Validation;
		TestTrue(Name + TEXT(" is valid"), Class->IsDataValid(Validation) != EDataValidationResult::Invalid && Validation.GetNumErrors() == 0);
#endif
		const TSubclassOf<UGameplayEffect> Effect = Class->AttributeEffect.LoadSynchronous();
		const UGameplayEffect* EffectDefaults = Effect ? Effect->GetDefaultObject<UGameplayEffect>() : nullptr;
		TestTrue(Name + TEXT(": an infinite effect with attribute modifiers"), EffectDefaults && EffectDefaults->DurationPolicy == EGameplayEffectDurationType::Infinite
			&& !EffectDefaults->Modifiers.IsEmpty());
		TestNotNull(*(Name + TEXT(": its ability set")), Class->AbilitySet.LoadSynchronous());
		TestTrue(Name + TEXT(": known to the profile rules"), Terms.Classes.Contains(Class->GetPrimaryAssetId()));
		for (const FHollowLoadoutSlot& Slot : Class->StartingLoadout)
		{
			TestTrue(Name + TEXT(": starting loadout of known raid items ") + Slot.Item.ToString(), Terms.Catalog.Contains(Slot.Item)
				&& Settings->LoadoutItemTypes.HasTag(Terms.Catalog.GetItemType(Slot.Item)));
		}
	}
	TestTrue(TEXT("The Engineer starts with the welder insured"), !Engineer->StartingLoadout.IsEmpty() && Engineer->StartingLoadout[0].bInsured);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
