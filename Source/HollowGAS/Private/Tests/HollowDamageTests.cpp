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

	void Hit(FHollowGASTestRig& Rig, const UHollowDamageEffect* Effect, const float Damage, const float Penetration = -1.f, const float ExposedBonus = -1.f)
	{
		FGameplayEffectSpec Spec(Effect, Rig.AbilitySystem->MakeEffectContext(), 1.f);
		Spec.SetSetByCallerMagnitude(HollowTags::Data_Damage, Damage);
		if (Penetration >= 0.f)
		{
			Spec.SetSetByCallerMagnitude(HollowTags::Data_ArmorPenetration, Penetration);
		}
		if (ExposedBonus >= 0.f)
		{
			Spec.SetSetByCallerMagnitude(HollowTags::Data_ExposedPenetration, ExposedBonus);
		}
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
	// GP-01: a hit ignores a share of the armour.
	TestEqual(TEXT("No penetration: as before"), Mitigate(Base, Armor, K, true, false, 0.f), Mitigate(Base, Armor, K, true, false));
	TestEqual(TEXT("Half the armour ignored"), Mitigate(Base, Armor, K, true, false, 0.5f), Base * K / (K + Armor * 0.5f));
	TestEqual(TEXT("All of it ignored"), Mitigate(Base, Armor, K, true, false, 1.f), Base);
	TestEqual(TEXT("Penetration clamps to 0..1"), Mitigate(Base, Armor, K, true, false, 3.f), Base);
	TestEqual(TEXT("...and never adds armour"), Mitigate(Base, Armor, K, true, false, -1.f), Mitigate(Base, Armor, K, true, false));
	TestEqual(TEXT("Immunity still wins"), Mitigate(Base, Armor, K, true, true, 1.f), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowDamagePenetrationTest, "Hollow.GAS.Damage.Penetration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowDamagePenetrationTest::RunTest(const FString& Parameters)
{
	// GP-01 (ADR-0044): the weapon's penetration comes with the hit (SetByCaller), its exposed bonus only counts on a
	// target in an Aether lantern's light (Hollow.State.Exposed); effects without them hit as before.
	using namespace HollowDamageTests;
	const UHollowDamageSettings* Settings = GetDefault<UHollowDamageSettings>();
	FHollowGASTestRig Rig;
	if (!Rig.Create<UHollowHealthSet>(*this) || !TestTrue(TEXT("Armor curve configured"), Settings->ArmorConstant > 0.f))
	{
		return false;
	}
	const float KValue = Settings->ArmorConstant;
	auto Health = [&Rig]() { return Rig.Value(UHollowHealthSet::GetHealthAttribute()); };
	auto Reset = [&Rig]()
	{
		Rig.Apply(UHollowHealthSet::GetMaxHealthAttribute(), EGameplayModOp::Override, MaxHealth);
		Rig.Apply(UHollowHealthSet::GetHealthAttribute(), EGameplayModOp::Override, MaxHealth);
		Rig.Apply(UHollowHealthSet::GetArmorAttribute(), EGameplayModOp::Override, Armor);
	};
	const UHollowDamageEffect* KineticHit = MakeEffect(HollowTags::Damage_Kinetic);
	constexpr float Penetration = 0.5f;
	constexpr float Bonus = 0.4f;

	Reset();
	Hit(Rig, KineticHit, Base);
	TestTrue(TEXT("Without SetByCaller: the armour as before"), FMath::IsNearlyEqual(Health(), MaxHealth - Base * KValue / (KValue + Armor), 1e-3f));
	Reset();
	Hit(Rig, KineticHit, Base, Penetration, Bonus);
	TestTrue(TEXT("Penetration: half the armour ignored; the bonus waits for an exposed target"),
		FMath::IsNearlyEqual(Health(), MaxHealth - Base * KValue / (KValue + Armor * (1.f - Penetration)), 1e-3f));
	Reset();
	Rig.AbilitySystem->AddLooseGameplayTag(HollowTags::State_Exposed);
	Hit(Rig, KineticHit, Base, Penetration, Bonus);
	TestTrue(TEXT("Exposed: penetration and bonus together"),
		FMath::IsNearlyEqual(Health(), MaxHealth - Base * KValue / (KValue + Armor * (1.f - Penetration - Bonus)), 1e-3f));
	Reset();
	Hit(Rig, KineticHit, Base, 0.8f, 0.8f);
	TestTrue(TEXT("Exposed: never more than the whole armour"), FMath::IsNearlyEqual(Health(), MaxHealth - Base, 1e-3f));
	Rig.AbilitySystem->RemoveLooseGameplayTag(HollowTags::State_Exposed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowDamageMarkerTest, "Hollow.GAS.Damage.Marker",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowDamageMarkerTest::RunTest(const FString& Parameters)
{
	// GP-01c (ADR-0046): the shooter's hit marker from what it knows of the target - a hit, armour that took most of it
	// (or immunity), a crit on an exposed target - by the same rules the server's execution uses.
	using namespace HollowDamageTests;
	using HollowDamageMath::Classify;
	constexpr float Share = 0.35f;
	FHollowDamageOutcome Outcome;
	Outcome.Damage = Base * 0.8f;
	TestEqual(TEXT("Most of it hurt: a hit"), Classify(Base, Outcome, Share), EHollowHitKind::Hit);
	Outcome.Damage = Base * 0.6f;
	TestEqual(TEXT("Armour took 40 %: armour"), Classify(Base, Outcome, Share), EHollowHitKind::Armor);
	Outcome.bExposed = true;
	TestEqual(TEXT("Exposed: a crit, armour or not"), Classify(Base, Outcome, Share), EHollowHitKind::Crit);
	Outcome.bImmune = true;
	Outcome.Damage = 0.f;
	TestEqual(TEXT("Immune: armour, exposed or not"), Classify(Base, Outcome, Share), EHollowHitKind::Armor);
	TestEqual(TEXT("Nothing hit: no marker"), Classify(0.f, Outcome, Share), EHollowHitKind::None);

	FHollowGASTestRig Rig;
	if (!Rig.Create<UHollowHealthSet>(*this))
	{
		return false;
	}
	const UHollowDamageSettings* Settings = GetDefault<UHollowDamageSettings>();
	const FHollowDamageTypeRule* Kinetic = Settings->FindRule(HollowTags::Damage_Kinetic);
	if (!TestNotNull(TEXT("A kinetic rule"), Kinetic) || !TestTrue(TEXT("A marker share"), Settings->ArmorMarkerShare > 0.f))
	{
		return false;
	}
	using HollowDamageRules::Preview;
	TestEqual(TEXT("No ability system: no marker"), Preview(nullptr, HollowTags::Damage_Kinetic, Base, 0.f, 0.f), EHollowHitKind::None);
	Rig.Apply(UHollowHealthSet::GetMaxHealthAttribute(), EGameplayModOp::Override, MaxHealth);
	Rig.Apply(UHollowHealthSet::GetHealthAttribute(), EGameplayModOp::Override, MaxHealth);
	Rig.Apply(UHollowHealthSet::GetArmorAttribute(), EGameplayModOp::Override, 0.f);
	TestEqual(TEXT("Bare: a hit"), Preview(Rig.AbilitySystem, HollowTags::Damage_Kinetic, Base, 0.f, 0.f), EHollowHitKind::Hit);
	Rig.Apply(UHollowHealthSet::GetArmorAttribute(), EGameplayModOp::Override, Armor * 3.f);   // a Pilgrim's plate
	TestEqual(TEXT("Plated: armour"), Preview(Rig.AbilitySystem, HollowTags::Damage_Kinetic, Base, 0.1f, 0.2f), EHollowHitKind::Armor);
	TestEqual(TEXT("...not to occult damage"), Preview(Rig.AbilitySystem, HollowTags::Damage_Occult, Base, 0.f, 0.f), EHollowHitKind::Hit);
	TestEqual(TEXT("...the same as the execution"), Preview(Rig.AbilitySystem, HollowTags::Damage_Kinetic, Base, 1.f, 0.f), EHollowHitKind::Hit);
	Rig.AbilitySystem->AddLooseGameplayTag(HollowTags::State_Exposed);
	TestEqual(TEXT("Exposed: a crit"), Preview(Rig.AbilitySystem, HollowTags::Damage_Kinetic, Base, 0.1f, 0.2f), EHollowHitKind::Crit);
	Rig.AbilitySystem->AddLooseGameplayTag(Kinetic->ImmunityTag);
	TestEqual(TEXT("Immune: armour even exposed"), Preview(Rig.AbilitySystem, HollowTags::Damage_Kinetic, Base, 0.1f, 0.2f), EHollowHitKind::Armor);
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

	// S4-06: Hollow.State.Invulnerable (exact) stops every type; the stalker's child tag does not widen to it.
	Reset(0.f);
	Rig.AbilitySystem->AddLooseGameplayTag(HollowTags::State_Invulnerable);
	Hit(Rig, KineticHit, Base);
	Hit(Rig, OccultHit, Base);
	TestEqual(TEXT("Invulnerable: no damage of any type"), Health(), MaxHealth);
	Rig.AbilitySystem->RemoveLooseGameplayTag(HollowTags::State_Invulnerable);
	Rig.AbilitySystem->AddLooseGameplayTag(HollowTags::State_Invulnerable_Kinetic);
	Hit(Rig, OccultHit, Base);
	TestEqual(TEXT("Kinetic immunity is not invulnerability"), Health(), MaxHealth - Base);
	Rig.AbilitySystem->RemoveLooseGameplayTag(HollowTags::State_Invulnerable_Kinetic);

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
