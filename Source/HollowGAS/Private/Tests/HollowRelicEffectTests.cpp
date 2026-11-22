#include "Tests/HollowGASTestRig.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Attributes/HollowGearSet.h"
#include "Attributes/HollowResonanceSet.h"
#include "Tags/HollowGameplayTags.h"

namespace HollowRelicEffectTests
{
	// Content under test (create_relic_assets.py, create_class_assets.py, create_lantern_assets.py, create_ward_assets.py).
	const TCHAR* const RingClass = TEXT("/Game/Hollow/Relics/GE_Relic_RingOfSilence.GE_Relic_RingOfSilence_C");
	const TCHAR* const StabilizerClass = TEXT("/Game/Hollow/Relics/GE_Relic_ResonanceStabilizer.GE_Relic_ResonanceStabilizer_C");
	const TCHAR* const CapacitorClass = TEXT("/Game/Hollow/Relics/GE_Relic_PlasmaCapacitor.GE_Relic_PlasmaCapacitor_C");
	const TCHAR* const BraceClass = TEXT("/Game/Hollow/Relics/GE_Relic_KineticBrace.GE_Relic_KineticBrace_C");
	const TCHAR* const OccultistClass = TEXT("/Game/Hollow/Classes/Effects/GE_Class_Occultist.GE_Class_Occultist_C");
	const TCHAR* const AuraClass = TEXT("/Game/Hollow/GAS/Effects/GE_AntiResonance.GE_AntiResonance_C");
	const TCHAR* const CalmClass = TEXT("/Game/Hollow/GAS/Effects/GE_Ward_Calm.GE_Ward_Calm_C");

	// S6-04 (ADR-0037): what each relic is meant to do (the task's numbers).
	constexpr float RingFactor = 0.7f;
	constexpr float StabilizerFactor = 0.75f;
	constexpr float CapacitorFactor = 1.2f;
	constexpr float BraceFactor = 1.15f;
	constexpr float Neutral = 1.f;
	constexpr float Tolerance = 1.0e-4f;

	float FirstFactor(const TSubclassOf<UGameplayEffect>& Effect)
	{
		float Value = 0.f;
		const UGameplayEffect* Defaults = Effect.GetDefaultObject();
		return Defaults->Modifiers.IsEmpty() || !Defaults->Modifiers[0].ModifierMagnitude.GetStaticMagnitudeIfPossible(1.f, Value) ? 0.f : Value;
	}

	FActiveGameplayEffectHandle Apply(UAbilitySystemComponent& AbilitySystem, const TSubclassOf<UGameplayEffect>& Effect)
	{
		return AbilitySystem.ApplyGameplayEffectToSelf(Effect.GetDefaultObject(), 1.f, AbilitySystem.MakeEffectContext());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowRelicEffectsTest, "Hollow.GAS.Relics.Effects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowRelicEffectsTest::RunTest(const FString& Parameters)
{
	// ADR-0037: each relic's effect lasts while it is carried and changes one multiplier by the task's number: the Ring
	// of Silence footstep noise x0.7, the Resonance Stabilizer gain x0.75, the Plasma Capacitor tool work x1.2, the
	// Kinetic Brace carry speed x1.15; all neutral without it. They multiply on top of the class, the lantern and the ward.
	using namespace HollowRelicEffectTests;
	struct FRelic
	{
		const TCHAR* Path;
		FGameplayAttribute Attribute;
		float Factor;
	};
	const FRelic Relics[] = {
		{RingClass, UHollowGearSet::GetFootstepNoiseMultiplierAttribute(), RingFactor},
		{StabilizerClass, UHollowResonanceSet::GetResonanceGainMultiplierAttribute(), StabilizerFactor},
		{CapacitorClass, UHollowGearSet::GetToolWorkMultiplierAttribute(), CapacitorFactor},
		{BraceClass, UHollowGearSet::GetCarrySpeedMultiplierAttribute(), BraceFactor},
	};
	FHollowGASTestRig Rig;
	if (!Rig.Create<UHollowGearSet, UHollowResonanceSet>(*this))
	{
		return false;
	}
	UHollowAbilitySystemComponent& AbilitySystem = *Rig.AbilitySystem;
	Rig.Apply(UHollowResonanceSet::GetResonanceGainMultiplierAttribute(), EGameplayModOp::Override, Neutral);
	TestTrue(TEXT("Without relics the gear is neutral"), Rig.Value(UHollowGearSet::GetFootstepNoiseMultiplierAttribute()) == Neutral
		&& Rig.Value(UHollowGearSet::GetToolWorkMultiplierAttribute()) == Neutral && Rig.Value(UHollowGearSet::GetCarrySpeedMultiplierAttribute()) == Neutral);

	for (const FRelic& Relic : Relics)
	{
		const TSubclassOf<UGameplayEffect> Effect = LoadClass<UGameplayEffect>(nullptr, Relic.Path);
		if (!TestNotNull(Relic.Path, Effect.Get()))
		{
			continue;
		}
		const FString Name = Effect->GetName();
		TestTrue(Name + TEXT(": lasts while carried, one modifier at the task's number"), Effect.GetDefaultObject()->DurationPolicy == EGameplayEffectDurationType::Infinite
			&& Effect.GetDefaultObject()->Modifiers.Num() == 1 && Effect.GetDefaultObject()->Modifiers[0].Attribute == Relic.Attribute
			&& FMath::IsNearlyEqual(FirstFactor(Effect), Relic.Factor, Tolerance));
		const FActiveGameplayEffectHandle Handle = Apply(AbilitySystem, Effect);
		TestNearlyEqual(Name + TEXT(": carried, its multiplier"), Rig.Value(Relic.Attribute), Relic.Factor, Tolerance);
		AbilitySystem.RemoveActiveGameplayEffect(Handle);
		TestNearlyEqual(Name + TEXT(": put away, neutral again"), Rig.Value(Relic.Attribute), Neutral, Tolerance);
	}

	// The stabilizer on an Occultist in a lantern's aura, standing in a ward: every factor multiplies.
	const TSubclassOf<UGameplayEffect> Stabilizer = LoadClass<UGameplayEffect>(nullptr, StabilizerClass);
	const TSubclassOf<UGameplayEffect> Occultist = LoadClass<UGameplayEffect>(nullptr, OccultistClass);
	const TSubclassOf<UGameplayEffect> Aura = LoadClass<UGameplayEffect>(nullptr, AuraClass);
	const TSubclassOf<UGameplayEffect> Calm = LoadClass<UGameplayEffect>(nullptr, CalmClass);
	if (TestNotNull(TEXT("GE_Class_Occultist"), Occultist.Get()) && TestNotNull(TEXT("GE_AntiResonance"), Aura.Get()) && TestNotNull(TEXT("GE_Ward_Calm"), Calm.Get())
		&& Stabilizer)
	{
		Apply(AbilitySystem, Occultist);
		const float ClassGain = Rig.Value(UHollowResonanceSet::GetResonanceGainMultiplierAttribute());
		Apply(AbilitySystem, Stabilizer);
		Apply(AbilitySystem, Aura);
		Apply(AbilitySystem, Calm);
		TestNearlyEqual(TEXT("Class, stabilizer, lantern and ward multiply"), Rig.Value(UHollowResonanceSet::GetResonanceGainMultiplierAttribute()),
			ClassGain * StabilizerFactor * FirstFactor(Aura) * FirstFactor(Calm), Tolerance);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
