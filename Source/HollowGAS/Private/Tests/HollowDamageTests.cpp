#include "Tests/HollowGASTestRig.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Attributes/HollowHealthSet.h"
#include "Damage/HollowDamageExecution.h"
#include "Damage/HollowDamageMath.h"
#include "Damage/HollowDamageSettings.h"
#include "Tags/HollowGameplayTags.h"

namespace HollowDamageTests
{
	// Test data (the shipped rules live in DefaultGame.ini).
	constexpr float Base = 60.f;
	constexpr float K = 100.f;
	constexpr float Armor = 100.f;
	constexpr float MaxHealth = 100.f;

	UHollowDamageEffect* MakeEffect(const FGameplayTag& DamageType)
	{
		UHollowDamageEffect* Effect = NewObject<UHollowDamageEffect>(GetTransientPackage());
		Effect->DamageType = DamageType;
		return Effect;
	}

	void Hit(FHollowGASTestRig& Rig, const UHollowDamageEffect* Effect, const float Damage)
	{
		FGameplayEffectSpec Spec(Effect, Rig.AbilitySystem->MakeEffectContext(), 1.f);
		Spec.SetSetByCallerMagnitude(HollowTags::Data_Damage, Damage);
		Rig.AbilitySystem->ApplyGameplayEffectSpecToSelf(Spec);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowDamageMathTest, "Hollow.GAS.Damage.Math",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowDamageMathTest::RunTest(const FString& Parameters)
{
	using namespace HollowDamageTests;
	using HollowDamageMath::Mitigate;
	TestEqual(TEXT("Armor equal to K halves the damage"), Mitigate(Base, K, K, true, false), Base * 0.5f);
	TestEqual(TEXT("No armor: full damage"), Mitigate(Base, 0.f, K, true, false), Base);
	TestEqual(TEXT("Negative armor counts as none"), Mitigate(Base, -Armor, K, true, false), Base);
	TestEqual(TEXT("Armor that does not apply"), Mitigate(Base, Armor, K, false, false), Base);
	TestEqual(TEXT("No curve (K = 0): full damage"), Mitigate(Base, Armor, 0.f, true, false), Base);
	TestEqual(TEXT("Immune"), Mitigate(Base, 0.f, K, true, true), 0.f);
	TestEqual(TEXT("Nothing negative"), Mitigate(-Base, 0.f, K, true, false), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowDamageExecutionTest, "Hollow.GAS.Damage.Execution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowDamageExecutionTest::RunTest(const FString& Parameters)
{
	// Damage only through a gameplay effect (CLAUDE.md sec. 4): SetByCaller base, the type's rules from the settings,
	// the target's Armor captured at execution, IncomingDamage applied by the health set.
	using namespace HollowDamageTests;
	const UHollowDamageSettings* Settings = GetDefault<UHollowDamageSettings>();
	const FHollowDamageTypeRule* Kinetic = Settings->FindRule(HollowTags::Damage_Kinetic);
	const FHollowDamageTypeRule* Occult = Settings->FindRule(HollowTags::Damage_Occult);
	FHollowGASTestRig Rig;
	if (!TestNotNull(TEXT("Kinetic rule in DefaultGame.ini"), Kinetic) || !TestNotNull(TEXT("Occult rule in DefaultGame.ini"), Occult)
		|| !TestTrue(TEXT("Armor curve configured"), Settings->ArmorConstant > 0.f) || !Rig.Create<UHollowHealthSet>(*this))
	{
		return false;
	}
	auto Health = [&Rig]() { return Rig.Value(UHollowHealthSet::GetHealthAttribute()); };
	auto Reset = [&Rig](const float NewArmor)
	{
		Rig.Apply(UHollowHealthSet::GetMaxHealthAttribute(), EGameplayModOp::Override, MaxHealth);
		Rig.Apply(UHollowHealthSet::GetHealthAttribute(), EGameplayModOp::Override, MaxHealth);
		Rig.Apply(UHollowHealthSet::GetArmorAttribute(), EGameplayModOp::Override, NewArmor);
	};
	int32 Deaths = 0;
	UHollowHealthSet* HealthSet = const_cast<UHollowHealthSet*>(Rig.AbilitySystem->GetSet<UHollowHealthSet>());
	HealthSet->OnOutOfHealth.AddLambda([&Deaths](AActor*, AActor*, float) { ++Deaths; });
	const UHollowDamageEffect* KineticHit = MakeEffect(HollowTags::Damage_Kinetic);
	const UHollowDamageEffect* OccultHit = MakeEffect(HollowTags::Damage_Occult);

	Reset(0.f);
	Hit(Rig, KineticHit, Base);
	TestEqual(TEXT("No armor: the whole hit"), Health(), MaxHealth - Base);

	Reset(Armor);
	Hit(Rig, KineticHit, Base);
	const float Expected = HollowDamageMath::Mitigate(Base, Armor, Settings->ArmorConstant, Kinetic->bArmorApplies, false);
	TestTrue(TEXT("Armor is captured and reduces kinetic damage"), Expected < Base && FMath::IsNearlyEqual(Health(), MaxHealth - Expected, KINDA_SMALL_NUMBER));

	Reset(Armor);
	Hit(Rig, OccultHit, Base);
	TestEqual(TEXT("Occult damage ignores armor"), Health(), MaxHealth - Base);

	Reset(0.f);
	Rig.AbilitySystem->AddLooseGameplayTag(Kinetic->ImmunityTag);
	Hit(Rig, KineticHit, Base);
	TestEqual(TEXT("Immune to kinetic damage"), Health(), MaxHealth);
	Hit(Rig, OccultHit, Base);
	TestEqual(TEXT("...but not to occult damage"), Health(), MaxHealth - Base);
	Rig.AbilitySystem->RemoveLooseGameplayTag(Kinetic->ImmunityTag);

	Reset(0.f);
	Hit(Rig, KineticHit, -Base);
	TestEqual(TEXT("A negative hit heals nothing"), Health(), MaxHealth);
	Hit(Rig, KineticHit, MaxHealth * 2.f);
	TestEqual(TEXT("Health stops at 0"), Health(), 0.f);
	TestEqual(TEXT("Out of health once"), Deaths, 1);
	Hit(Rig, KineticHit, Base);
	TestEqual(TEXT("No second out-of-health"), Deaths, 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
