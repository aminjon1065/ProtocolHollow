#include "Tests/HollowGameTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Attributes/HollowResonanceSet.h"
#include "Camera/CameraComponent.h"
#include "Carry/HollowAetherCore.h"
#include "Carry/HollowCarryableComponent.h"
#include "Carry/HollowCarryableDefinition.h"
#include "Effects/HollowResonanceGainCalculation.h"
#include "Enemies/HollowStalkerDefinition.h"
#include "Framework/HollowGameState.h"
#include "GameplayEffect.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Net/UnrealNetwork.h"
#include "Resonance/HollowResonanceConfig.h"
#include "Resonance/HollowResonanceMath.h"
#include "Resonance/HollowResonancePresentationConfig.h"
#include "Resonance/HollowResonancePresentationSubsystem.h"
#include "Resonance/HollowSquadResonanceComponent.h"
#include "Sound/SoundClass.h"
#include "SoundControlBus.h"
#include "Tags/HollowGameplayTags.h"
#include "UObject/Package.h"

namespace HollowResonanceGameTests
{
	// Content under test (create_resonance_assets.py, create_carry_assets.py, create_stalker_assets.py).
	const TCHAR* const CoreClass = TEXT("/Game/Hollow/Items/AetherCore/BP_AetherCore.BP_AetherCore_C");
	const TCHAR* const InitEffectClass = TEXT("/Game/Hollow/GAS/Effects/GE_ExoFrame_InitAttributes.GE_ExoFrame_InitAttributes_C");
	const TCHAR* const StalkerDefinition = TEXT("/Game/Hollow/Characters/Enemies/EchoStalker/DA_Enemy_EchoStalker.DA_Enemy_EchoStalker");
	const TCHAR* const ExoFrameClass = TEXT("/Game/Hollow/Characters/ExoFrame/BP_ExoFrame.BP_ExoFrame_C");
	const TCHAR* const PostProcess = TEXT("/Game/Hollow/FX/Resonance/M_PP_Resonance.M_PP_Resonance");
	const TCHAR* const WorldSoundClass = TEXT("/Game/Hollow/Audio/SC_Hollow_World.SC_Hollow_World");

	// Test data.
	constexpr float TickSeconds = 1.f / 60.f;
	constexpr float Max = 100.f;
	constexpr float UpdateSeconds = 0.5f;
	constexpr float Slack = 0.1f;
	constexpr float Tolerance = 1.0e-3f;
	constexpr float CoreDistance = 120.f;
	constexpr int32 Periods = 3;

	void Advance(FTestWorldWrapper& World, const float Seconds, const TFunctionRef<void()> EveryFrame = [] {})
	{
		for (float Elapsed = 0.f; Elapsed < Seconds; Elapsed += TickSeconds)
		{
			EveryFrame();
			++GFrameCounter;
			World.TickTestWorld(TickSeconds);
		}
	}

	UHollowResonanceConfig* MakeConfig()
	{
		UHollowResonanceConfig* Config = NewObject<UHollowResonanceConfig>(GetTransientPackage());
		Config->TierThresholds = {0.25f, 0.5f, 0.75f};
		Config->Hysteresis = 0.03f;
		Config->UpdateSeconds = UpdateSeconds;
		return Config;
	}

	/** A raid's game state, playing, its squad on the test tuning (spawned first: player states join it). */
	AHollowGameState* SpawnGameState(UWorld* World)
	{
		AHollowGameState* GameState = World->SpawnActor<AHollowGameState>();
		GameState->GetSquadResonance()->SetConfig(MakeConfig());
		if (!GameState->HasActorBegunPlay())
		{
			GameState->DispatchBeginPlay();
		}
		return GameState;
	}

	void Set(UAbilitySystemComponent* AbilitySystem, const FGameplayAttribute& Attribute, const float Value)
	{
		UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
		Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
		FGameplayModifierInfo& Modifier = Effect->Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Override;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Value));
		AbilitySystem->ApplyGameplayEffectToSelf(Effect, 1.f, AbilitySystem->MakeEffectContext());
	}

	/** A player of the game state with Resonance Value of Max. */
	UAbilitySystemComponent* SpawnMember(UWorld* World, const float Value)
	{
		AHollowPlayerState* Player = World->SpawnActor<AHollowPlayerState>();
		UAbilitySystemComponent* AbilitySystem = Player->GetAbilitySystemComponent();
		AbilitySystem->InitAbilityActorInfo(Player, Player);
		Set(AbilitySystem, UHollowResonanceSet::GetMaxResonanceAttribute(), Max);
		Set(AbilitySystem, UHollowResonanceSet::GetResonanceAttribute(), Value);
		return AbilitySystem;
	}

	float Resonance(const UAbilitySystemComponent* AbilitySystem)
	{
		return AbilitySystem->GetNumericAttribute(UHollowResonanceSet::GetResonanceAttribute());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowResonanceSquadTest, "Hollow.Game.Resonance.Squad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowResonanceSquadTest::RunTest(const FString& Parameters)
{
	// UHollowSquadResonanceComponent (ADR-0020): the server takes the most exposed living player, as a byte and a
	// tier; it changes on its timer only (no faster than UpdateSeconds) and only when the value does, push-replicated.
	using namespace HollowResonanceGameTests;
	FTestWorldWrapper TestWorld;
	UWorld* World = HollowGameTests::CreateWorld(*this, TestWorld);
	if (!World)
	{
		return false;
	}
	AHollowGameState* GameState = SpawnGameState(World);
	UHollowSquadResonanceComponent* Squad = GameState->GetSquadResonance();
	TestTrue(TEXT("The game state's squad is found"), UHollowSquadResonanceComponent::Get(World) == Squad);
	UAbilitySystemComponent* Carrier = SpawnMember(World, Max * 0.3f);
	UAbilitySystemComponent* Other = SpawnMember(World, Max * 0.1f);
	int32 Changes = 0;
	Squad->OnSquadResonanceChanged.AddLambda([&Changes](const FHollowSquadResonanceNetState&) { ++Changes; });

	Squad->Evaluate();
	TestEqual(TEXT("The most exposed player"), Squad->GetState().Level, HollowResonanceMath::Quantize(0.3f));
	TestTrue(TEXT("...Medium"), Squad->GetTier() == EHollowResonanceTier::Medium);
	TestEqual(TEXT("...announced once"), Changes, 1);
	Squad->Evaluate();
	TestEqual(TEXT("No change, no news"), Changes, 1);
	Cast<UHollowAbilitySystemComponent>(Carrier)->AddLooseGameplayTag(HollowTags::State_Dead);
	Squad->Evaluate();
	TestEqual(TEXT("The dead do not count"), Squad->GetState().Level, HollowResonanceMath::Quantize(0.1f));
	TestTrue(TEXT("...Low"), Squad->GetTier() == EHollowResonanceTier::Low);

	// The timer: no faster than UpdateSeconds, however often it changes.
	Set(Other, UHollowResonanceSet::GetResonanceAttribute(), Max * 0.6f);
	TestEqual(TEXT("Not before the timer"), Squad->GetState().Level, HollowResonanceMath::Quantize(0.1f));
	Advance(TestWorld, UpdateSeconds + Slack);
	TestEqual(TEXT("On the timer"), Squad->GetState().Level, HollowResonanceMath::Quantize(0.6f));
	TestTrue(TEXT("...High"), Squad->GetTier() == EHollowResonanceTier::High);
	const int32 Before = Changes;
	constexpr float RampSeconds = 2.f;
	float Ramp = 0.f;
	Advance(TestWorld, RampSeconds, [&Ramp, Other] { Ramp += 0.2f; Set(Other, UHollowResonanceSet::GetResonanceAttribute(), Ramp); });
	TestTrue(FString::Printf(TEXT("Changing every frame for %.0f s: %d changes, at most one per UpdateSeconds"), RampSeconds, Changes - Before),
		Changes - Before <= FMath::CeilToInt32(RampSeconds / UpdateSeconds) + 1);

	TArray<FLifetimeProperty> Replicated;
	static_cast<const UObject*>(Squad)->GetLifetimeReplicatedProps(Replicated);   // public on UObject
	const FProperty* State = FindFProperty<FProperty>(UHollowSquadResonanceComponent::StaticClass(), TEXT("State"));
	const FLifetimeProperty* Entry = Replicated.FindByPredicate([State](const FLifetimeProperty& P) { return State && P.RepIndex == State->RepIndex; });
	TestTrue(TEXT("The state replicates to everyone, push-based"), Entry && Entry->Condition == COND_None && Entry->bIsPushBased);
	TestTrue(TEXT("...2 bytes on the wire"), sizeof(FHollowSquadResonanceNetState) == 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowResonanceCoreCarryTest, "Hollow.Game.Resonance.CoreCarry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowResonanceCoreCarryTest::RunTest(const FString& Parameters)
{
	// The real BP_AetherCore and GE_Carry_Solo: carrying raises the carrier's Resonance every period, scaled by its
	// ResonanceGainMultiplier; the squad follows.
	using namespace HollowResonanceGameTests;
	FTestWorldWrapper TestWorld;
	UWorld* World = HollowGameTests::CreateWorld(*this, TestWorld);
	const TSubclassOf<AHollowAetherCore> CoreBlueprint = LoadClass<AHollowAetherCore>(nullptr, CoreClass);
	const TSubclassOf<UGameplayEffect> Init = LoadClass<UGameplayEffect>(nullptr, InitEffectClass);
	if (!World || !TestNotNull(TEXT("BP_AetherCore"), CoreBlueprint.Get()) || !TestNotNull(TEXT("GE_ExoFrame_InitAttributes"), Init.Get()))
	{
		return false;
	}
	UHollowSquadResonanceComponent* Squad = SpawnGameState(World)->GetSquadResonance();
	AHollowExoFrameCharacter* Character = HollowGameTests::SpawnExoFrame(*this, World);
	HollowGameTests::PossessAsPlayer(World, Character);
	Character->DispatchBeginPlay();
	UAbilitySystemComponent* AbilitySystem = Character->GetAbilitySystemComponent();
	AbilitySystem->ApplyGameplayEffectToSelf(Init.GetDefaultObject(), 1.f, AbilitySystem->MakeEffectContext());
	AHollowAetherCore* Core = World->SpawnActor<AHollowAetherCore>(CoreBlueprint, FTransform(FVector(CoreDistance, 0.0, 0.0)));
	Core->DispatchBeginPlay();
	const UHollowCarryableDefinition* Definition = Core->GetCarryable()->GetDefinition();
	if (!TestNotNull(TEXT("Core definition"), Definition) || !TestNotNull(TEXT("Solo effect"), Definition->SoloEffect.Get()))
	{
		return false;
	}
	const float Period = Definition->SoloEffect.GetDefaultObject()->Period.GetValue();
	TestEqual(TEXT("Fresh: no Resonance"), Resonance(AbilitySystem), 0.f);

	Core->Interact(Character);
	Advance(TestWorld, Period * 0.5f);
	const float Start = Resonance(AbilitySystem);
	Advance(TestWorld, Period * Periods);
	const float Gain = (Resonance(AbilitySystem) - Start) / Periods;
	TestTrue(FString::Printf(TEXT("Carrying gains Resonance every period (%.2f per %.1f s)"), Gain, Period), Gain > 0.f);
	constexpr float Doubled = 2.f;
	Set(AbilitySystem, UHollowResonanceSet::GetResonanceGainMultiplierAttribute(), Doubled);
	const float Boosted = Resonance(AbilitySystem);
	Advance(TestWorld, Period * Periods);
	TestNearlyEqual(TEXT("...times the carrier's gain multiplier"), (Resonance(AbilitySystem) - Boosted) / Periods, Gain * Doubled, Tolerance);
	Squad->Evaluate();
	TestEqual(TEXT("The squad follows its carrier"), Squad->GetState().Level, HollowResonanceMath::Quantize(Resonance(AbilitySystem) / Max));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowResonancePresentationTest, "Hollow.Game.Resonance.Presentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowResonancePresentationTest::RunTest(const FString& Parameters)
{
	// UHollowResonancePresentationSubsystem with the shipped DA_ResonancePresentation: it follows the squad value,
	// eases towards it over EaseSeconds on a timer that stops when there, writes MPC_Resonance (intensity, server-time
	// offset) and the bus value.
	using namespace HollowResonanceGameTests;
	FTestWorldWrapper TestWorld;
	UWorld* World = HollowGameTests::CreateWorld(*this, TestWorld);
	UHollowResonancePresentationSubsystem* Presentation = UHollowResonancePresentationSubsystem::Get(World);
	const UHollowResonancePresentationConfig* Config = Presentation ? Presentation->GetConfig() : nullptr;
	if (!World || !TestNotNull(TEXT("Presentation on this machine"), Presentation) || !TestNotNull(TEXT("DA_ResonancePresentation"), Config))
	{
		return false;
	}
	UHollowSquadResonanceComponent* Squad = SpawnGameState(World)->GetSquadResonance();   // the game state arriving attaches it
	UAbilitySystemComponent* Member = SpawnMember(World, Max);
	Squad->Evaluate();
	const float Step = Config->StepSeconds / Config->EaseSeconds;
	TestEqual(TEXT("Target: the squad's value"), Presentation->GetTargetIntensity(), 1.f);
	TestTrue(TEXT("...no pop: still dark, easing"), Presentation->GetIntensity() < Step && Presentation->IsEasing());
	TestNearlyEqual(TEXT("This machine's offset to server time (0 on the server)"), Presentation->GetTimeOffset(), 0.f, Tolerance);
	TestNearlyEqual(TEXT("...written into the collection"), Presentation->ReadCollectionScalar(Config->TimeOffsetParameter), Presentation->GetTimeOffset(), Tolerance);

	Advance(TestWorld, Config->EaseSeconds * 0.5f);
	TestNearlyEqual(TEXT("Half way after half the ease"), Presentation->GetIntensity(), 0.5f, Step * 2.f);
	TestNearlyEqual(TEXT("MPC intensity is what is shown"), Presentation->ReadCollectionScalar(Config->IntensityParameter), Presentation->GetIntensity(), Tolerance);
	Advance(TestWorld, Config->EaseSeconds * 0.5f + Config->StepSeconds * 4.f);
	TestEqual(TEXT("There after EaseSeconds"), Presentation->GetIntensity(), 1.f);
	TestFalse(TEXT("...and the timer stopped"), Presentation->IsEasing());
	TestNearlyEqual(TEXT("Full: the bus at its full value"), Presentation->GetBusValue(), Config->BusValueFull, Tolerance);

	Set(Member, UHollowResonanceSet::GetResonanceAttribute(), 0.f);
	Squad->Evaluate();
	Advance(TestWorld, Config->EaseSeconds + Config->StepSeconds * 4.f);
	TestTrue(TEXT("Back to calm"), Presentation->GetIntensity() == 0.f && !Presentation->IsEasing()
		&& FMath::IsNearlyEqual(Presentation->GetBusValue(), Config->BusValueCalm, Tolerance));
	TestEqual(TEXT("...MPC too"), Presentation->ReadCollectionScalar(Config->IntensityParameter), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowResonanceDataTest, "Hollow.Game.Resonance.Data",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowResonanceDataTest::RunTest(const FString& Parameters)
{
	// The shipped Resonance data: tiers, the server rate (<= 2 Hz, Block 3), the stalker's hearing curve, the gain
	// through the calculation, the post-process on the camera, the bus and the sound class it muffles.
	using namespace HollowResonanceGameTests;
	const UHollowResonanceConfig* Config = UHollowResonanceConfig::Get();
	const UHollowResonancePresentationConfig* Presentation = UHollowResonancePresentationConfig::Get();
	const UHollowStalkerDefinition* Stalker = LoadObject<UHollowStalkerDefinition>(nullptr, StalkerDefinition);
	if (!TestNotNull(TEXT("DA_ResonanceConfig"), Config) || !TestNotNull(TEXT("DA_ResonancePresentation"), Presentation)
		|| !TestNotNull(TEXT("DA_Enemy_EchoStalker"), Stalker))
	{
		return false;
	}
	bool bTiers = Config->TierThresholds.Num() == 3;
	for (int32 Index = 0; Index < Config->TierThresholds.Num(); ++Index)
	{
		bTiers &= Config->TierThresholds[Index] > 0.f && Config->TierThresholds[Index] < 1.f
			&& (Index == 0 || Config->TierThresholds[Index] - Config->TierThresholds[Index - 1] > Config->Hysteresis);
	}
	TestTrue(TEXT("Three tiers above Low, apart by more than the hysteresis"), bTiers);
	constexpr float MaxRateHz = 2.f;
	TestTrue(TEXT("The squad value changes no faster than 2 Hz"), Config->UpdateSeconds >= 1.f / MaxRateHz);

	FString Problem;
	const FRichCurve* Hearing = Stalker->HearingResonanceCurve.GetRichCurveConst();
	TestTrue(FString::Printf(TEXT("Stalker hearing: x1 at 0, never decreasing (%s)"), *Problem), Hearing && HollowResonanceMath::IsValidMultiplierCurve(*Hearing, 1.f, Problem));
	TestTrue(TEXT("...and it does grow"), HollowResonanceMath::EvaluateMultiplier(Stalker->HearingResonanceCurve, 1.f) > 1.f);

	const TSubclassOf<AHollowAetherCore> CoreBlueprint = LoadClass<AHollowAetherCore>(nullptr, CoreClass);
	const AHollowAetherCore* Core = CoreBlueprint ? CoreBlueprint.GetDefaultObject() : nullptr;
	const UHollowCarryableDefinition* Carry = Core && Core->GetCarryable() ? Core->GetCarryable()->GetDefinition() : nullptr;
	const UGameplayEffect* Solo = Carry && Carry->SoloEffect ? Carry->SoloEffect.GetDefaultObject() : nullptr;
	const FGameplayModifierInfo* Gain = Solo ? Solo->Modifiers.FindByPredicate([](const FGameplayModifierInfo& M) { return M.Attribute == UHollowResonanceSet::GetResonanceAttribute(); }) : nullptr;
	TestTrue(TEXT("GE_Carry_Solo gains through UHollowResonanceGainCalculation"), Gain
		&& Gain->ModifierMagnitude.GetMagnitudeCalculationType() == EGameplayEffectMagnitudeCalculation::CustomCalculationClass
		&& Gain->ModifierMagnitude.GetCustomMagnitudeCalculationClass() == UHollowResonanceGainCalculation::StaticClass());

	const UMaterialParameterCollection* Collection = Presentation->Collection.LoadSynchronous();
	TestTrue(TEXT("MPC_Resonance holds both scalars"), Collection && Collection->GetScalarParameterByName(Presentation->IntensityParameter)
		&& Collection->GetScalarParameterByName(Presentation->TimeOffsetParameter));
	const UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, PostProcess);
	const TSubclassOf<AHollowExoFrameCharacter> ExoFrame = LoadClass<AHollowExoFrameCharacter>(nullptr, ExoFrameClass);
	const UCameraComponent* Camera = ExoFrame ? ExoFrame.GetDefaultObject()->GetFirstPersonCamera() : nullptr;
	TestTrue(TEXT("BP_ExoFrame's camera blends M_PP_Resonance in full"), Material && Camera
		&& Camera->PostProcessSettings.WeightedBlendables.Array.ContainsByPredicate([Material](const FWeightedBlendable& B) { return B.Object == Material && B.Weight == 1.f; }));
	const USoundControlBus* Bus = Presentation->Bus.LoadSynchronous();
	const USoundClass* SoundClass = LoadObject<USoundClass>(nullptr, WorldSoundClass);
	TestTrue(TEXT("SC_Hollow_World's low-pass follows CB_Resonance"), Bus && SoundClass
		&& SoundClass->Properties.ModulationSettings.LowpassModulationDestination.Modulators.Contains(Bus));
	TestTrue(TEXT("Calm is the bus parameter's default (no effect)"), Presentation->BusValueCalm == 1.f && Presentation->BusValueFull < Presentation->BusValueCalm);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
