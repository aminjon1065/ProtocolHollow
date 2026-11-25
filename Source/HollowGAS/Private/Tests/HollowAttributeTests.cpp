#include "Tests/HollowGASTestRig.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Attributes/HollowGearSet.h"
#include "Attributes/HollowHealthSet.h"
#include "Attributes/HollowMovementSet.h"
#include "Attributes/HollowResonanceSet.h"
#include "Attributes/HollowSurvivalSet.h"
#include "Attributes/HollowToolSet.h"
#include "Attributes/HollowWeaponSet.h"
#include "Net/UnrealNetwork.h"

namespace HollowAttributeTests
{
	// Test data only: real values live in GE_ExoFrame_InitAttributes.
	constexpr float Max = 100.f;
	constexpr float Half = 50.f;
	constexpr float Huge = 1000.f;
	constexpr float High = 80.f;
	constexpr float LoweredMax = 30.f;
	constexpr float Hit = 30.f;
	constexpr float Heal = 40.f;

	using EModOp = EGameplayModOp::Type;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowAttributeClampTest, "Hollow.GAS.AttributeClamp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowAttributeClampTest::RunTest(const FString& Parameters)
{
	using namespace HollowAttributeTests;

	FHollowGASTestRig Rig;
	if (!Rig.Create<UHollowHealthSet, UHollowSurvivalSet, UHollowResonanceSet, UHollowToolSet>(*this))
	{
		return false;
	}

	const UHollowAttributeSetBase* Sets[] = {
		Rig.AbilitySystem->GetSet<UHollowHealthSet>(), Rig.AbilitySystem->GetSet<UHollowSurvivalSet>(),
		Rig.AbilitySystem->GetSet<UHollowResonanceSet>(), Rig.AbilitySystem->GetSet<UHollowToolSet>() };

	int32 CheckedPairs = 0;
	for (const UHollowAttributeSetBase* Set : Sets)
	{
		if (!TestNotNull(TEXT("Attribute set is registered"), Set))
		{
			return false;
		}

		for (const FHollowBoundedAttribute& Bounded : Set->GetBoundedAttributes())
		{
			const FString Name = Bounded.Current.GetName();
			Rig.Apply(Bounded.Max, EModOp::Override, Max);
			Rig.Apply(Bounded.Current, EModOp::Override, Half);
			TestEqual(Name + TEXT(" accepts values inside the range"), Rig.Value(Bounded.Current), Half);

			Rig.Apply(Bounded.Current, EModOp::AddBase, Huge);
			TestEqual(Name + TEXT(" never exceeds its max"), Rig.Value(Bounded.Current), Max);

			Rig.Apply(Bounded.Current, EModOp::AddBase, -Huge);
			TestEqual(Name + TEXT(" never drops below zero"), Rig.Value(Bounded.Current), 0.f);

			Rig.Apply(Bounded.Current, EModOp::Override, High);
			Rig.Apply(Bounded.Max, EModOp::Override, LoweredMax);
			TestEqual(Name + TEXT(" follows a lowered max"), Rig.Value(Bounded.Current), LoweredMax);

			Rig.Apply(Bounded.Max, EModOp::Override, -Max);
			TestEqual(Bounded.Max.GetName() + TEXT(" never drops below zero"), Rig.Value(Bounded.Max), 0.f);
			TestEqual(Name + TEXT(" follows a zero max"), Rig.Value(Bounded.Current), 0.f);
			++CheckedPairs;
		}
	}
	TestTrue(TEXT("Every set exposes at least one bounded pair"), CheckedPairs >= static_cast<int32>(UE_ARRAY_COUNT(Sets)));

	// Unbounded attributes stay non-negative.
	for (const FGameplayAttribute& Attribute : { UHollowHealthSet::GetArmorAttribute(), UHollowResonanceSet::GetResonanceGainMultiplierAttribute() })
	{
		Rig.Apply(Attribute, EModOp::Override, -Max);
		TestEqual(Attribute.GetName() + TEXT(" never drops below zero"), Rig.Value(Attribute), 0.f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowMetaDamageTest, "Hollow.GAS.MetaDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowMetaDamageTest::RunTest(const FString& Parameters)
{
	using namespace HollowAttributeTests;

	FHollowGASTestRig Rig;
	if (!Rig.Create<UHollowHealthSet>(*this))
	{
		return false;
	}

	const FGameplayAttribute Health = UHollowHealthSet::GetHealthAttribute();
	const FGameplayAttribute Damage = UHollowHealthSet::GetIncomingDamageAttribute();
	const FGameplayAttribute Healing = UHollowHealthSet::GetIncomingHealingAttribute();
	Rig.Apply(UHollowHealthSet::GetMaxHealthAttribute(), EModOp::Override, Max);
	Rig.Apply(Health, EModOp::Override, Max);

	Rig.Apply(Damage, EModOp::AddBase, Hit);
	TestEqual(TEXT("Damage lowers health"), Rig.Value(Health), Max - Hit);
	TestEqual(TEXT("IncomingDamage is consumed"), Rig.Value(Damage), 0.f);

	Rig.Apply(Damage, EModOp::AddBase, -Hit);
	TestEqual(TEXT("Negative damage does not heal"), Rig.Value(Health), Max - Hit);

	Rig.Apply(Damage, EModOp::AddBase, Huge);
	TestEqual(TEXT("Overkill stops at zero"), Rig.Value(Health), 0.f);

	Rig.Apply(Healing, EModOp::AddBase, Heal);
	TestEqual(TEXT("Healing raises health"), Rig.Value(Health), Heal);
	TestEqual(TEXT("IncomingHealing is consumed"), Rig.Value(Healing), 0.f);

	Rig.Apply(Healing, EModOp::AddBase, Huge);
	TestEqual(TEXT("Overheal stops at max"), Rig.Value(Health), Max);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowAttributeReplicationTest, "Hollow.GAS.ReplicationConditions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowAttributeReplicationTest::RunTest(const FString& Parameters)
{
	// CLAUDE.md sec. 3 bandwidth hygiene: private resources only reach their owner, meta attributes never replicate.
	struct FExpectation
	{
		UClass* Class;
		TArray<FName> Replicated;
		ELifetimeCondition Condition;
		TArray<FName> NotReplicated;
	};
	const FExpectation Expectations[] = {
		{ UHollowHealthSet::StaticClass(), { TEXT("Health"), TEXT("MaxHealth"), TEXT("Armor") }, COND_None, { TEXT("IncomingDamage"), TEXT("IncomingHealing") } },
		{ UHollowResonanceSet::StaticClass(), { TEXT("Resonance"), TEXT("MaxResonance"), TEXT("ResonanceGainMultiplier") }, COND_None, {} },
		{ UHollowSurvivalSet::StaticClass(), { TEXT("Stamina"), TEXT("MaxStamina"), TEXT("Oxygen"), TEXT("MaxOxygen"), TEXT("Thermal"), TEXT("MaxThermal") }, COND_OwnerOnly, {} },
		{ UHollowToolSet::StaticClass(), { TEXT("ToolFuel"), TEXT("MaxToolFuel"), TEXT("ToolHeat"), TEXT("MaxToolHeat") }, COND_OwnerOnly, {} },
		// S6-02: enemies move on the server only, so their speed multiplier never leaves it.
		{ UHollowMovementSet::StaticClass(), {}, COND_None, { TEXT("MoveSpeedMultiplier") } },
		// S6-04: the gear's multipliers are the owner's (its carry speed is predicted there).
		{ UHollowGearSet::StaticClass(), { TEXT("FootstepNoiseMultiplier"), TEXT("ToolWorkMultiplier"), TEXT("CarrySpeedMultiplier") }, COND_OwnerOnly, {} },
		// GP-01: the weapon's magazine is the owner's (its shots predicted there).
		{ UHollowWeaponSet::StaticClass(), { TEXT("Magazine"), TEXT("MaxMagazine") }, COND_OwnerOnly, {} },
	};

	for (const FExpectation& Expectation : Expectations)
	{
		Expectation.Class->SetUpRuntimeReplicationData();   // ClassReps is otherwise built lazily on first replication
		TArray<FLifetimeProperty> Lifetime;
		Expectation.Class->GetDefaultObject()->GetLifetimeReplicatedProps(Lifetime);

		TMap<FName, const FLifetimeProperty*> ByName;
		for (const FLifetimeProperty& Property : Lifetime)
		{
			if (!Expectation.Class->ClassReps.IsValidIndex(Property.RepIndex))
			{
				AddError(FString::Printf(TEXT("%s: replicated property index %d is unknown."), *Expectation.Class->GetName(), Property.RepIndex));
				continue;
			}
			ByName.Add(Expectation.Class->ClassReps[Property.RepIndex].Property->GetFName(), &Property);
		}

		for (const FName& Name : Expectation.Replicated)
		{
			const FLifetimeProperty* const* Found = ByName.Find(Name);
			const FString Label = Expectation.Class->GetName() + TEXT(".") + Name.ToString();
			if (TestNotNull(Label + TEXT(" is replicated"), Found))
			{
				TestTrue(Label + TEXT(" uses the expected condition"), (*Found)->Condition == Expectation.Condition);
				TestTrue(Label + TEXT(" always notifies"), (*Found)->RepNotifyCondition == REPNOTIFY_Always);
			}
		}
		for (const FName& Name : Expectation.NotReplicated)
		{
			TestFalse(Expectation.Class->GetName() + TEXT(".") + Name.ToString() + TEXT(" is not replicated"), ByName.Contains(Name));
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
