#include "Tests/HollowGASTestRig.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Attributes/HollowHealthSet.h"
#include "Attributes/HollowMovementSet.h"
#include "Attributes/HollowResonanceSet.h"
#include "Attributes/HollowSurvivalSet.h"
#include "Effects/HollowOccultEffects.h"
#include "Tags/HollowGameplayTags.h"

namespace HollowAetherLanternEffectTests
{
	// Content under test (create_lantern_assets.py, create_stalker_assets.py, create_class_assets.py, create_carry_assets.py).
	const TCHAR* const ExposeClass = TEXT("/Game/Hollow/GAS/Effects/GE_Occult_Expose.GE_Occult_Expose_C");
	const TCHAR* const AuraClass = TEXT("/Game/Hollow/GAS/Effects/GE_AntiResonance.GE_AntiResonance_C");
	const TCHAR* const TraitsClass = TEXT("/Game/Hollow/GAS/Effects/GE_Stalker_Traits.GE_Stalker_Traits_C");
	const TCHAR* const OccultistClass = TEXT("/Game/Hollow/Classes/Effects/GE_Class_Occultist.GE_Class_Occultist_C");
	const TCHAR* const CarryClass = TEXT("/Game/Hollow/GAS/Effects/GE_Carry_Solo.GE_Carry_Solo_C");

	// Test data.
	constexpr float TickSeconds = 1.f / 60.f;
	constexpr float Armor = 100.f;
	constexpr float Plenty = 1000.f;   // stamina and Resonance room: nothing runs out or caps meanwhile
	constexpr float Neutral = 1.f;
	constexpr float Tolerance = 1.0e-3f;
	constexpr float RatioTolerance = 0.02f;

	float Seconds(const TSubclassOf<UGameplayEffect>& Effect)
	{
		float Value = 0.f;
		Effect.GetDefaultObject()->DurationMagnitude.GetStaticMagnitudeIfPossible(1.f, Value);
		return Value;
	}

	/** The magnitude of an effect's first modifier (the aura's factor). */
	float FirstFactor(const TSubclassOf<UGameplayEffect>& Effect)
	{
		float Value = 0.f;
		const UGameplayEffect* Defaults = Effect.GetDefaultObject();
		return Defaults->Modifiers.IsEmpty() || !Defaults->Modifiers[0].ModifierMagnitude.GetStaticMagnitudeIfPossible(1.f, Value) ? 0.f : Value;
	}

	/** One engine frame per step (durations and periods follow the timer manager); Each runs before every frame. */
	void Advance(FHollowGASTestRig& Rig, const float Duration, const TFunctionRef<void()> Each = [] {})
	{
		for (float Elapsed = 0.f; Elapsed < Duration; Elapsed += TickSeconds)
		{
			Each();
			++GFrameCounter;
			Rig.World.TickTestWorld(TickSeconds);
		}
	}

	/** Another actor with an ability system in the rig's world: a lantern held by someone else. */
	UHollowAbilitySystemComponent* MakeSource(FHollowGASTestRig& Rig)
	{
		AActor* Holder = Rig.World.GetTestWorld()->SpawnActor<AActor>();
		UHollowAbilitySystemComponent* AbilitySystem = NewObject<UHollowAbilitySystemComponent>(Holder, TEXT("AbilitySystem"));
		AbilitySystem->RegisterComponent();
		AbilitySystem->InitAbilityActorInfo(Holder, Holder);
		return AbilitySystem;
	}

	void ApplyFrom(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target, const TSubclassOf<UGameplayEffect>& Effect)
	{
		const FGameplayEffectSpecHandle Spec = Source.MakeOutgoingSpec(Effect, 1.f, Source.MakeEffectContext());
		Source.ApplyGameplayEffectSpecToTarget(*Spec.Data, &Target);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowAetherLanternEffectsTest, "Hollow.GAS.AetherLantern.Effects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowAetherLanternEffectsTest::RunTest(const FString& Parameters)
{
	// ADR-0035: the lantern's effects. Exposed: slowed, weaker armour, one instance whoever lights it (a renewal restarts
	// its time), then it fades; the stalker's kinetic immunity is held back meanwhile. The aura multiplies the Resonance
	// gain on top of the class's, so a carried core's Resonance builds at the aura's factor.
	using namespace HollowAetherLanternEffectTests;
	const TSubclassOf<UGameplayEffect> Expose = LoadClass<UGameplayEffect>(nullptr, ExposeClass);
	const TSubclassOf<UGameplayEffect> Aura = LoadClass<UGameplayEffect>(nullptr, AuraClass);
	const TSubclassOf<UGameplayEffect> Traits = LoadClass<UGameplayEffect>(nullptr, TraitsClass);
	const TSubclassOf<UGameplayEffect> Occultist = LoadClass<UGameplayEffect>(nullptr, OccultistClass);
	const TSubclassOf<UGameplayEffect> Carry = LoadClass<UGameplayEffect>(nullptr, CarryClass);
	FHollowGASTestRig Rig;
	if (!TestNotNull(TEXT("GE_Occult_Expose"), Expose.Get()) || !TestNotNull(TEXT("GE_AntiResonance"), Aura.Get())
		|| !TestNotNull(TEXT("GE_Stalker_Traits"), Traits.Get()) || !TestNotNull(TEXT("GE_Class_Occultist"), Occultist.Get())
		|| !TestNotNull(TEXT("GE_Carry_Solo"), Carry.Get())
		|| !Rig.Create<UHollowHealthSet, UHollowMovementSet, UHollowResonanceSet, UHollowSurvivalSet>(*this))
	{
		return false;
	}
	TestTrue(TEXT("The lantern's effects have its structure"), Expose->IsChildOf<UHollowOccultExposeEffect>() && Aura->IsChildOf<UHollowAntiResonanceEffect>()
		&& Seconds(Expose) > 0.f && Seconds(Aura) > 0.f);
	TestTrue(TEXT("...a renewal triggers no cue again (no RPC per lantern sample)"), Expose.GetDefaultObject()->bSuppressStackingCues
		&& Aura.GetDefaultObject()->bSuppressStackingCues);
	UHollowAbilitySystemComponent& Target = *Rig.AbilitySystem;
	UHollowAbilitySystemComponent* First = MakeSource(Rig);
	UHollowAbilitySystemComponent* Second = MakeSource(Rig);
	const FGameplayAttribute Speed = UHollowMovementSet::GetMoveSpeedMultiplierAttribute();
	const FGameplayAttribute ArmorAttribute = UHollowHealthSet::GetArmorAttribute();
	Rig.Apply(ArmorAttribute, EGameplayModOp::Override, Armor);
	TestEqual(TEXT("Nothing slows an enemy yet"), Rig.Value(Speed), Neutral);

	// In the light; a second lantern renews the same instance.
	ApplyFrom(*First, Target, Expose);
	const float Slowed = Rig.Value(Speed);
	TestTrue(TEXT("In the light: exposed, slowed, weaker armour"), Target.HasMatchingGameplayTag(HollowTags::State_Exposed) && Slowed < Neutral
		&& Slowed > 0.f && Rig.Value(ArmorAttribute) < Armor && Rig.Value(ArmorAttribute) > 0.f);
	Advance(Rig, Seconds(Expose) * 0.75f);
	ApplyFrom(*Second, Target, Expose);
	TestEqual(TEXT("Two lanterns: one instance"), Target.GetGameplayEffectCount(Expose, nullptr), 1);
	TestNearlyEqual(TEXT("...that slows once"), Rig.Value(Speed), Slowed, Tolerance);
	Advance(Rig, Seconds(Expose) * 0.75f);
	TestTrue(TEXT("A renewal restarts its time"), Target.HasMatchingGameplayTag(HollowTags::State_Exposed));
	Advance(Rig, Seconds(Expose) * 0.5f);
	TestTrue(TEXT("...then it fades: speed and armour back"), !Target.HasMatchingGameplayTag(HollowTags::State_Exposed)
		&& FMath::IsNearlyEqual(Rig.Value(Speed), Neutral) && FMath::IsNearlyEqual(Rig.Value(ArmorAttribute), Armor));

	// The stalker's traits are held back in the light, not removed.
	ApplyFrom(Target, Target, Traits);
	TestTrue(TEXT("The stalker's traits: kinetic immunity"), Target.HasMatchingGameplayTag(HollowTags::State_Invulnerable_Kinetic));
	ApplyFrom(*First, Target, Expose);
	TestTrue(TEXT("In the light: no immunity, the traits only held back"), !Target.HasMatchingGameplayTag(HollowTags::State_Invulnerable_Kinetic)
		&& Target.GetGameplayEffectCount(Traits, nullptr, /*bEnforceOnGoingCheck=*/false) == 1 && Target.GetGameplayEffectCount(Traits, nullptr) == 0);
	Advance(Rig, Seconds(Expose) + TickSeconds * 2.f);
	TestTrue(TEXT("Out of the light: immune again"), Target.HasMatchingGameplayTag(HollowTags::State_Invulnerable_Kinetic));

	// The aura on top of the Occultist's own slower Resonance.
	const FGameplayAttribute Gain = UHollowResonanceSet::GetResonanceGainMultiplierAttribute();
	Rig.Apply(Gain, EGameplayModOp::Override, Neutral);
	Rig.Apply(UHollowResonanceSet::GetMaxResonanceAttribute(), EGameplayModOp::Override, Plenty);
	Rig.Apply(UHollowSurvivalSet::GetMaxStaminaAttribute(), EGameplayModOp::Override, Plenty);
	Rig.Apply(UHollowSurvivalSet::GetStaminaAttribute(), EGameplayModOp::Override, Plenty);
	ApplyFrom(Target, Target, Occultist);
	const float ClassGain = Rig.Value(Gain);
	const float Factor = FirstFactor(Aura);
	ApplyFrom(*First, Target, Aura);
	TestTrue(TEXT("In the aura: its tag"), Target.HasMatchingGameplayTag(HollowTags::State_AntiResonance) && Factor > 0.f && Factor < Neutral);
	TestNearlyEqual(TEXT("...and the gain multiplied on top of the class's"), Rig.Value(Gain), ClassGain * Factor, Tolerance);
	Advance(Rig, Seconds(Aura) + TickSeconds * 2.f);
	TestTrue(TEXT("Out of the aura: the class's gain again"), !Target.HasMatchingGameplayTag(HollowTags::State_AntiResonance)
		&& FMath::IsNearlyEqual(Rig.Value(Gain), ClassGain));

	// Carrying a core: two periods in the aura (renewed every frame, as lantern samples would), two out of it, each
	// window half a period off the ticks so it holds exactly two.
	ApplyFrom(Target, Target, Carry);
	const float Period = Carry.GetDefaultObject()->Period.GetValue();
	auto Resonance = [&Rig] { return Rig.Value(UHollowResonanceSet::GetResonanceAttribute()); };
	auto KeepAura = [&] { ApplyFrom(*First, Target, Aura); };
	Advance(Rig, Period * 0.5f, KeepAura);
	const float AuraStart = Resonance();
	Advance(Rig, Period * 2.f, KeepAura);
	const float InAura = Resonance() - AuraStart;
	Advance(Rig, FMath::CeilToFloat(Seconds(Aura) / Period) * Period + Period);   // the aura gone, the same phase
	const float OutStart = Resonance();
	Advance(Rig, Period * 2.f);
	const float OutOfAura = Resonance() - OutStart;
	TestTrue(FString::Printf(TEXT("The carrier gains Resonance (%.2f in the aura, %.2f out of it)"), InAura, OutOfAura), InAura > 0.f && OutOfAura > InAura);
	TestNearlyEqual(TEXT("...in the aura at the aura's factor"), OutOfAura > 0.f ? InAura / OutOfAura : 0.f, Factor, RatioTolerance);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
