#include "Tests/HollowGameTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/HollowAbilitySet.h"
#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Audio/HollowAudioSubsystem.h"
#include "Components/SpotLightComponent.h"
#include "Gadgets/HollowGA_HelmetLamp.h"
#include "GameplayEffect.h"
#include "GameplayPrediction.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Interfaces/HollowLightSource.h"
#include "Net/UnrealNetwork.h"
#include "Tags/HollowGameplayTags.h"

namespace HollowHelmetLampTests
{
	// Content under test (create_lamp_assets.py).
	const TCHAR* const LampAbilityClass = TEXT("/Game/Hollow/Characters/ExoFrame/GA_HelmetLamp.GA_HelmetLamp_C");
	const TCHAR* const AbilitySet = TEXT("/Game/Hollow/Characters/ExoFrame/DA_AbilitySet_ExoFrame.DA_AbilitySet_ExoFrame");
	const TCHAR* const LampAction = TEXT("/Game/Hollow/Core/Input/Actions/IA_HelmetLamp.IA_HelmetLamp");
	const TCHAR* const RaidMappingContext = TEXT("/Game/Hollow/Core/Input/IMC_Raid.IMC_Raid");

	constexpr float TickSeconds = 1.f / 60.f;
	/** No strobing: at most three flashes (off and on again) a second, the photosensitivity limit the visor keeps too. */
	constexpr float MaxFlashesPerSecond = 3.f;
	constexpr float SwitchesPerFlash = 2.f;

	void Advance(FTestWorldWrapper& World, const float Seconds)
	{
		for (float Elapsed = 0.f; Elapsed < Seconds; Elapsed += TickSeconds)
		{
			++GFrameCounter;
			World.TickTestWorld(TickSeconds);
		}
	}

	float CooldownSeconds(const UGameplayAbility* Ability)
	{
		float Seconds = 0.f;
		const UGameplayEffect* Cooldown = Ability ? Ability->GetCooldownGameplayEffect() : nullptr;
		return Cooldown && Cooldown->DurationMagnitude.GetStaticMagnitudeIfPossible(1.f, Seconds) ? Seconds : 0.f;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowHelmetLampSwitchTest, "Hollow.Game.HelmetLamp.Switch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowHelmetLampSwitchTest::RunTest(const FString& Parameters)
{
	// GA_HelmetLamp (S4-08, ADR-0026): on at spawn; the ability switches it, its light and the cone the Echo-Stalker
	// avoids; a cooldown keeps it from strobing; the dead cannot switch; the state replicates to all but the owner, who
	// predicts it and switches back what the server rejects.
	using namespace HollowHelmetLampTests;
	FTestWorldWrapper TestWorld;
	UWorld* World = HollowGameTests::CreateWorld(*this, TestWorld);
	const TSubclassOf<UGameplayAbility> AbilityClass = LoadClass<UGameplayAbility>(nullptr, LampAbilityClass);
	AHollowExoFrameCharacter* Character = HollowGameTests::SpawnExoFrame(*this, World);
	if (!TestNotNull(TEXT("GA_HelmetLamp"), AbilityClass.Get()) || !TestNotNull(TEXT("ExoFrame"), Character))
	{
		return false;
	}
	// A local player sets up input; the bare C++ class has no InputConfig (BP_ExoFrame data), which it reports.
	AddExpectedMessage(TEXT("InputConfig is not set"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, /*Occurrences=*/0, /*IsRegex=*/false);
	HollowGameTests::PossessAsPlayer(World, Character, /*bLocalPlayer=*/true);
	Character->DispatchBeginPlay();
	UHollowAbilitySystemComponent* AbilitySystem = Character->GetHollowAbilitySystemComponent();
	AbilitySystem->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1));
	TArray<bool> Heard;
	Character->OnHelmetLampChanged.AddLambda([&Heard](const bool bOn) { Heard.Add(bOn); });
	UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(World);
	const int32 ClicksBefore = Audio ? Audio->GetPlayCount(HollowTags::Sound_HelmetLamp_Toggle) : 0;

	FHollowLightCone Cone;
	TestTrue(TEXT("On at spawn: lit, with a cone"), Character->IsHelmetLampOn() && Character->GetHelmetLight()->IsVisible() && Character->GetHelmetLampCone(Cone));
	TestTrue(TEXT("The ability switches it off"), AbilitySystem->TryActivateAbilityByClass(AbilityClass));
	TestTrue(TEXT("...dark, no cone for the AI"), !Character->IsHelmetLampOn() && !Character->GetHelmetLight()->IsVisible() && !Character->GetHelmetLampCone(Cone));
	TestTrue(TEXT("...the visor hears it"), Heard.Num() == 1 && !Heard[0]);
	if (Audio)
	{
		TestEqual(TEXT("...with a click"), Audio->GetPlayCount(HollowTags::Sound_HelmetLamp_Toggle) - ClicksBefore, 1);
	}
	TestTrue(TEXT("Cooling down"), AbilitySystem->HasMatchingGameplayTag(HollowTags::Cooldown_HelmetLamp));
	TestFalse(TEXT("...no switch meanwhile"), AbilitySystem->TryActivateAbilityByClass(AbilityClass));
	const float Cooldown = CooldownSeconds(AbilityClass.GetDefaultObject());
	TestTrue(FString::Printf(TEXT("The cooldown (%.2f s) allows at most %.0f flashes a second"), Cooldown, MaxFlashesPerSecond),
		Cooldown * SwitchesPerFlash >= 1.f / MaxFlashesPerSecond - UE_KINDA_SMALL_NUMBER);
	Advance(TestWorld, Cooldown + TickSeconds * 2.f);
	TestTrue(TEXT("After it: back on"), AbilitySystem->TryActivateAbilityByClass(AbilityClass) && Character->IsHelmetLampOn() && Character->GetHelmetLampCone(Cone));

	// The owner's prediction: a switch the server rejects is switched back; an accepted one stays.
	FPredictionKeyEvent Rejected;
	UHollowGA_HelmetLamp::PredictSwitch(*Character, Rejected);
	TestFalse(TEXT("Predicted: off at once"), Character->IsHelmetLampOn());
	Rejected.ExecuteIfBound();   // what the prediction key's rejection does
	TestTrue(TEXT("...rejected: on again"), Character->IsHelmetLampOn());
	FPredictionKeyEvent Accepted;
	UHollowGA_HelmetLamp::PredictSwitch(*Character, Accepted);
	TestTrue(TEXT("An accepted prediction stays (nothing fires)"), !Character->IsHelmetLampOn() && Accepted.IsBound());
	Character->SetHelmetLampOn(true);

	// What goes on the wire.
	TArray<FLifetimeProperty> Replicated;
	AHollowExoFrameCharacter::StaticClass()->SetUpRuntimeReplicationData();   // replication indices are set up lazily (by the first replication)
	static_cast<const UObject*>(Character)->GetLifetimeReplicatedProps(Replicated);   // public on UObject
	const FProperty* Lamp = FindFProperty<FProperty>(AHollowExoFrameCharacter::StaticClass(), TEXT("bHelmetLampOn"));
	const FLifetimeProperty* Entry = Replicated.FindByPredicate([Lamp](const FLifetimeProperty& P) { return Lamp && P.RepIndex == Lamp->RepIndex; });
	TestTrue(TEXT("The switch replicates to all but the owner (who predicts it), push-based"), Entry && Entry->Condition == COND_SkipOwner && Entry->bIsPushBased);

	// The dead: dark, and the switch is refused.
	Character->Die();
	TestTrue(TEXT("Dead: dark and no cone"), !Character->GetHelmetLight()->IsVisible() && !Character->GetHelmetLampCone(Cone));
	Advance(TestWorld, Cooldown + TickSeconds * 2.f);
	TestFalse(TEXT("...the dead cannot switch"), AbilitySystem->TryActivateAbilityByClass(AbilityClass));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowHelmetLampDataTest, "Hollow.Game.HelmetLamp.Data",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowHelmetLampDataTest::RunTest(const FString& Parameters)
{
	// The shipped switch: the ExoFrame is granted GA_HelmetLamp on Hollow.Input.HelmetLamp, mapped to F and the
	// D-pad's up in IMC_Raid (and nothing else on those keys there).
	using namespace HollowHelmetLampTests;
	const UHollowAbilitySet* Set = LoadObject<UHollowAbilitySet>(nullptr, AbilitySet);
	const TSubclassOf<UGameplayAbility> AbilityClass = LoadClass<UGameplayAbility>(nullptr, LampAbilityClass);
	const UInputAction* Action = LoadObject<UInputAction>(nullptr, LampAction);
	const UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, RaidMappingContext);
	if (!TestNotNull(TEXT("DA_AbilitySet_ExoFrame"), Set) || !TestNotNull(TEXT("GA_HelmetLamp"), AbilityClass.Get())
		|| !TestNotNull(TEXT("IA_HelmetLamp"), Action) || !TestNotNull(TEXT("IMC_Raid"), Context))
	{
		return false;
	}
	TestTrue(TEXT("The ExoFrame gets the lamp on its input"), Set->GrantedAbilities.ContainsByPredicate([&AbilityClass](const FHollowAbilitySetAbility& Entry)
	{
		return Entry.Ability.Get() == AbilityClass.Get() && Entry.InputTag == HollowTags::Input_HelmetLamp;
	}));
	TestTrue(TEXT("...a predicted switch with a cooldown"), AbilityClass->IsChildOf(UHollowGA_HelmetLamp::StaticClass()) && CooldownSeconds(AbilityClass.GetDefaultObject()) > 0.f);
	TArray<FKey> Keys;
	for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
	{
		if (Mapping.Action == Action)
		{
			Keys.Add(Mapping.Key);
		}
		else
		{
			TestFalse(FString::Printf(TEXT("%s is the lamp's alone"), *Mapping.Key.ToString()), Mapping.Key == EKeys::F || Mapping.Key == EKeys::Gamepad_DPad_Up);
		}
	}
	TestTrue(TEXT("F and the D-pad's up"), Keys.Num() == 2 && Keys.Contains(EKeys::F) && Keys.Contains(EKeys::Gamepad_DPad_Up));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
