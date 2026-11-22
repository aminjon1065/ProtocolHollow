#include "Tests/HollowGASTestRig.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Attributes/HollowHealthSet.h"
#include "Attributes/HollowMovementSet.h"
#include "Attributes/HollowResonanceSet.h"
#include "Effects/HollowOccultEffects.h"
#include "Tags/HollowGameplayTags.h"

namespace HollowOccultWardEffectTests
{
	// Content under test (create_ward_assets.py, create_lantern_assets.py, create_class_assets.py).
	const TCHAR* const BindClass = TEXT("/Game/Hollow/GAS/Effects/GE_Ward_Bind.GE_Ward_Bind_C");
	const TCHAR* const CalmClass = TEXT("/Game/Hollow/GAS/Effects/GE_Ward_Calm.GE_Ward_Calm_C");
	const TCHAR* const DrainClass = TEXT("/Game/Hollow/GAS/Effects/GE_Ward_Drain.GE_Ward_Drain_C");
	const TCHAR* const ExposeClass = TEXT("/Game/Hollow/GAS/Effects/GE_Occult_Expose.GE_Occult_Expose_C");
	const TCHAR* const OccultistClass = TEXT("/Game/Hollow/Classes/Effects/GE_Class_Occultist.GE_Class_Occultist_C");

	// Test data.
	constexpr float TickSeconds = 1.f / 60.f;
	constexpr float MaxResonance = 100.f;
	constexpr float SomeResonance = 10.f;
	constexpr float AlmostNone = 0.1f;
	constexpr float OneDrain = 0.25f;
	constexpr float Neutral = 1.f;
	constexpr float Tolerance = 1.0e-3f;

	float Seconds(const TSubclassOf<UGameplayEffect>& Effect)
	{
		float Value = 0.f;
		Effect.GetDefaultObject()->DurationMagnitude.GetStaticMagnitudeIfPossible(1.f, Value);
		return Value;
	}

	float FirstFactor(const TSubclassOf<UGameplayEffect>& Effect)
	{
		float Value = 0.f;
		const UGameplayEffect* Defaults = Effect.GetDefaultObject();
		return Defaults->Modifiers.IsEmpty() || !Defaults->Modifiers[0].ModifierMagnitude.GetStaticMagnitudeIfPossible(1.f, Value) ? 0.f : Value;
	}

	void Advance(FHollowGASTestRig& Rig, const float Duration)
	{
		for (float Elapsed = 0.f; Elapsed < Duration; Elapsed += TickSeconds)
		{
			++GFrameCounter;
			Rig.World.TickTestWorld(TickSeconds);
		}
	}

	UHollowAbilitySystemComponent* MakeSource(FHollowGASTestRig& Rig)
	{
		AActor* Holder = Rig.World.GetTestWorld()->SpawnActor<AActor>();
		UHollowAbilitySystemComponent* AbilitySystem = NewObject<UHollowAbilitySystemComponent>(Holder, TEXT("AbilitySystem"));
		AbilitySystem->RegisterComponent();
		AbilitySystem->InitAbilityActorInfo(Holder, Holder);
		return AbilitySystem;
	}

	void ApplyFrom(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target, const TSubclassOf<UGameplayEffect>& Effect, const float Drain = 0.f)
	{
		const FGameplayEffectSpecHandle Spec = Source.MakeOutgoingSpec(Effect, 1.f, Source.MakeEffectContext());
		Spec.Data->SetSetByCallerMagnitude(HollowTags::Data_ResonanceDrain, -Drain);
		Source.ApplyGameplayEffectSpecToTarget(*Spec.Data, &Target);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowOccultWardEffectsTest, "Hollow.GAS.OccultWard.Effects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowOccultWardEffectsTest::RunTest(const FString& Parameters)
{
	// ADR-0036: the ward's effects. Bound: slowed (on top of the lantern's light), one instance whoever's ward, a
	// renewal triggering no cue, gone after it lingers. Calmed: the Resonance gain multiplied on top of the class's. The
	// drain takes Resonance away as it is, whatever the gain multiplier, and never below zero.
	using namespace HollowOccultWardEffectTests;
	const TSubclassOf<UGameplayEffect> Bind = LoadClass<UGameplayEffect>(nullptr, BindClass);
	const TSubclassOf<UGameplayEffect> Calm = LoadClass<UGameplayEffect>(nullptr, CalmClass);
	const TSubclassOf<UGameplayEffect> Drain = LoadClass<UGameplayEffect>(nullptr, DrainClass);
	const TSubclassOf<UGameplayEffect> Expose = LoadClass<UGameplayEffect>(nullptr, ExposeClass);
	const TSubclassOf<UGameplayEffect> Occultist = LoadClass<UGameplayEffect>(nullptr, OccultistClass);
	FHollowGASTestRig Rig;
	if (!TestNotNull(TEXT("GE_Ward_Bind"), Bind.Get()) || !TestNotNull(TEXT("GE_Ward_Calm"), Calm.Get()) || !TestNotNull(TEXT("GE_Ward_Drain"), Drain.Get())
		|| !TestNotNull(TEXT("GE_Occult_Expose"), Expose.Get()) || !TestNotNull(TEXT("GE_Class_Occultist"), Occultist.Get())
		|| !Rig.Create<UHollowHealthSet, UHollowMovementSet, UHollowResonanceSet>(*this))
	{
		return false;
	}
	TestTrue(TEXT("The ward's effects: renewed area effects, no cue per renewal; the drain instant"), Bind->IsChildOf<UHollowWardBindEffect>()
		&& Calm->IsChildOf<UHollowWardCalmEffect>() && Bind.GetDefaultObject()->bSuppressStackingCues && Calm.GetDefaultObject()->bSuppressStackingCues
		&& Seconds(Bind) > 0.f && Seconds(Calm) > 0.f && Drain.GetDefaultObject()->DurationPolicy == EGameplayEffectDurationType::Instant);
	UHollowAbilitySystemComponent& Target = *Rig.AbilitySystem;
	UHollowAbilitySystemComponent* First = MakeSource(Rig);
	UHollowAbilitySystemComponent* Second = MakeSource(Rig);
	const FGameplayAttribute Speed = UHollowMovementSet::GetMoveSpeedMultiplierAttribute();

	// Bound: slowed, by two wards once, on top of the lantern; then it lingers out.
	ApplyFrom(*First, Target, Bind);
	ApplyFrom(*Second, Target, Bind);
	const float BindFactor = FirstFactor(Bind);
	TestTrue(TEXT("Inside a ward: warded, slowed"), Target.HasMatchingGameplayTag(HollowTags::State_Warded) && BindFactor > 0.f && BindFactor < Neutral);
	TestTrue(TEXT("...two wards: one instance, one slow"), Target.GetGameplayEffectCount(Bind, nullptr) == 1 && FMath::IsNearlyEqual(Rig.Value(Speed), BindFactor, Tolerance));
	ApplyFrom(*First, Target, Expose);
	TestNearlyEqual(TEXT("...and in the lantern's light too: both slows multiply"), Rig.Value(Speed), BindFactor * FirstFactor(Expose), Tolerance);
	Advance(Rig, FMath::Max(Seconds(Bind), Seconds(Expose)) + TickSeconds * 2.f);
	TestTrue(TEXT("Out of both: its speed back"), !Target.HasMatchingGameplayTag(HollowTags::State_Warded) && FMath::IsNearlyEqual(Rig.Value(Speed), Neutral));

	// Calmed: the gain on top of the class's.
	const FGameplayAttribute Gain = UHollowResonanceSet::GetResonanceGainMultiplierAttribute();
	const FGameplayAttribute Resonance = UHollowResonanceSet::GetResonanceAttribute();
	Rig.Apply(Gain, EGameplayModOp::Override, Neutral);
	Rig.Apply(UHollowResonanceSet::GetMaxResonanceAttribute(), EGameplayModOp::Override, MaxResonance);
	ApplyFrom(Target, Target, Occultist);
	const float ClassGain = Rig.Value(Gain);
	ApplyFrom(*First, Target, Calm);
	TestTrue(TEXT("Calmed: warded"), Target.HasMatchingGameplayTag(HollowTags::State_Warded));
	TestNearlyEqual(TEXT("...the gain multiplied on top of the class's"), Rig.Value(Gain), ClassGain * FirstFactor(Calm), Tolerance);

	// The drain: as it is, whatever the multiplier; never below zero.
	Rig.Apply(Resonance, EGameplayModOp::Override, SomeResonance);
	ApplyFrom(*First, Target, Drain, OneDrain);
	TestNearlyEqual(TEXT("A drain takes its share away, the gain multiplier aside"), Rig.Value(Resonance), SomeResonance - OneDrain, Tolerance);
	Rig.Apply(Resonance, EGameplayModOp::Override, AlmostNone);
	ApplyFrom(*First, Target, Drain, OneDrain);
	TestEqual(TEXT("...never below zero"), Rig.Value(Resonance), 0.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
