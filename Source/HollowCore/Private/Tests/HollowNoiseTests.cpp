#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Noise/HollowNoiseConfig.h"
#include "Noise/HollowNoiseSubsystem.h"
#include "Tags/HollowGameplayTags.h"
#include "Tests/AutomationCommon.h"
#include "UObject/Package.h"

namespace HollowNoiseTests
{
	// Test data (the shipped rules live in DA_NoiseConfig).
	constexpr float WelderLoudness = 1.f;
	constexpr float WelderRange = 1500.f;
	constexpr float WelderInterval = 0.5f;
	constexpr float StepLoudness = 0.25f;
	constexpr float MinSpeed = 200.f;
	constexpr float ReferenceSpeed = 400.f;
	constexpr float Sprint = 640.f;
	constexpr float Crouch = 180.f;
	constexpr float Half = 0.5f;

	FHollowNoiseRule Rule(const FGameplayTag& Tag, const float Loudness, const float Range, const float Interval)
	{
		FHollowNoiseRule Result;
		Result.NoiseTag = Tag;
		Result.Loudness = Loudness;
		Result.MaxRange = Range;
		Result.MinInterval = Interval;
		return Result;
	}

	UHollowNoiseConfig* MakeConfig()
	{
		UHollowNoiseConfig* Config = NewObject<UHollowNoiseConfig>(GetTransientPackage());
		Config->Rules = {Rule(HollowTags::Noise_Tool_Welder, WelderLoudness, WelderRange, WelderInterval), Rule(HollowTags::Noise_Footstep, StepLoudness, 0.f, 0.f)};
		Config->FootstepMinSpeed = MinSpeed;
		Config->FootstepReferenceSpeed = ReferenceSpeed;
		return Config;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowNoiseSubsystemTest, "Hollow.Core.Noise.Subsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowNoiseSubsystemTest::RunTest(const FString& Parameters)
{
	// Sources only say what and where; the rule of the tag decides loudness, range and rate (ADR-0018).
	using namespace HollowNoiseTests;
	FTestWorldWrapper TestWorld;
	if (!TestWorld.CreateTestWorld(EWorldType::Game))
	{
		TestWorld.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = TestWorld.GetTestWorld();
	World->InitializeActorsForPlay(FURL());
	UHollowNoiseSubsystem* Noise = UHollowNoiseSubsystem::Get(World);
	if (!TestNotNull(TEXT("Noise subsystem in a game world"), Noise))
	{
		return false;
	}
	const UHollowNoiseConfig* Shipped = Noise->GetConfig();
	if (TestNotNull(TEXT("DA_NoiseConfig loaded from the settings"), Shipped))
	{
		const FGameplayTag Made[] = {HollowTags::Noise_Tool_Welder, HollowTags::Noise_Tool_Cutter, HollowTags::Noise_Footstep,
			HollowTags::Noise_Core_Drop, HollowTags::Noise_Impact_Heavy, HollowTags::Noise_Structural_Breach};
		for (const FGameplayTag& Tag : Made)
		{
			TestNotNull(*FString::Printf(TEXT("The shipped config has a rule for %s"), *Tag.ToString()), Shipped->FindRule(Tag));
		}
	}

	Noise->SetConfig(MakeConfig());
	TArray<FHollowNoiseReport> Reports;
	Noise->OnNoiseReported.AddLambda([&Reports](const FHollowNoiseReport& Report) { Reports.Add(Report); });
	AActor* Welder = World->SpawnActor<AActor>();
	AActor* Other = World->SpawnActor<AActor>();
	const FVector Spot(100.0, 200.0, 0.0);

	TestTrue(TEXT("A welder's noise goes out"), Noise->ReportNoise(FHollowNoiseEvent(HollowTags::Noise_Tool_Welder, Spot, Welder)));
	if (TestEqual(TEXT("Reported once"), Reports.Num(), 1))
	{
		TestTrue(TEXT("With its rule's loudness and range"), Reports[0].Loudness == WelderLoudness && Reports[0].MaxRange == WelderRange);
		TestTrue(TEXT("Where and by whom"), Reports[0].Event.Location == Spot && Reports[0].Event.Instigator.Get() == Welder);
	}
	TestFalse(TEXT("Again too soon from the same welder: rate-limited"), Noise->ReportNoise(FHollowNoiseEvent(HollowTags::Noise_Tool_Welder, Spot, Welder)));
	TestTrue(TEXT("Another welder is not"), Noise->ReportNoise(FHollowNoiseEvent(HollowTags::Noise_Tool_Welder, Spot, Other)));
	TestTrue(TEXT("Another noise of the same welder is not"), Noise->ReportNoise(FHollowNoiseEvent(HollowTags::Noise_Footstep, Spot, Welder, Half)));
	TestTrue(TEXT("The scale multiplies the loudness"), Reports.Num() == 3 && FMath::IsNearlyEqual(Reports.Last().Loudness, StepLoudness * Half));
	TestFalse(TEXT("A silent noise is not reported"), Noise->ReportNoise(FHollowNoiseEvent(HollowTags::Noise_Footstep, Spot, Welder, 0.f)));
	for (float Elapsed = 0.f; Elapsed <= WelderInterval; Elapsed += WelderInterval * 0.25f)
	{
		TestWorld.TickTestWorld(WelderInterval * 0.25f);
	}
	TestTrue(TEXT("After the interval the welder is heard again"), Noise->ReportNoise(FHollowNoiseEvent(HollowTags::Noise_Tool_Welder, Spot, Welder)));

	AddExpectedMessage(TEXT("has no rule"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, /*Occurrences=*/1, /*IsRegex=*/false);
	TestFalse(TEXT("A tag without a rule is refused"), Noise->ReportNoise(FHollowNoiseEvent(HollowTags::Noise_Voice, Spot, Welder)));
	TestFalse(TEXT("...and warned about once"), Noise->ReportNoise(FHollowNoiseEvent(HollowTags::Noise_Voice, Spot, Welder)));
	TestFalse(TEXT("No tag, no noise"), Noise->ReportNoise(FHollowNoiseEvent(FGameplayTag(), Spot, Welder)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowNoiseAnswerTest, "Hollow.Core.Noise.Answer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowNoiseAnswerTest::RunTest(const FString& Parameters)
{
	// POLISH-01 (ADR-0053): an enemy that wakes to a player's noise answers it when the noise's rule raises an acoustic
	// alert, at most once per AnswerInterval per enemy; the player's visor hears of it at most once per
	// AcousticAlertCooldown, whichever enemy answered. The shipped rules: every noise but footsteps and the lantern.
	using namespace HollowNoiseTests;
	constexpr float AnswerInterval = 2.f;
	constexpr float AlertCooldown = 5.f;
	constexpr float Step = 0.25f;
	FTestWorldWrapper TestWorld;
	if (!TestWorld.CreateTestWorld(EWorldType::Game))
	{
		TestWorld.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = TestWorld.GetTestWorld();
	World->InitializeActorsForPlay(FURL());
	UHollowNoiseSubsystem* Noise = UHollowNoiseSubsystem::Get(World);
	if (!TestNotNull(TEXT("Noise subsystem"), Noise))
	{
		return false;
	}
	if (const UHollowNoiseConfig* Shipped = Noise->GetConfig(); TestNotNull(TEXT("DA_NoiseConfig"), Shipped))
	{
		for (const FHollowNoiseRule& Rule : Shipped->Rules)
		{
			const bool bSteady = Rule.NoiseTag == HollowTags::Noise_Footstep || Rule.NoiseTag == HollowTags::Noise_Tool_Lantern;
			TestEqual(*FString::Printf(TEXT("Shipped: %s raises an acoustic alert"), *Rule.NoiseTag.ToString()), Rule.bRaisesAcousticAlert, !bSteady);
		}
		TestTrue(TEXT("Shipped: a visor alert at most every 8-10 s"), Shipped->AcousticAlertCooldown >= 8.f && Shipped->AcousticAlertCooldown <= 10.f);
		TestTrue(TEXT("Shipped: an enemy answers at most every few seconds"), Shipped->AnswerInterval > 0.f);
	}

	UHollowNoiseConfig* Config = MakeConfig();
	Config->Rules[0].bRaisesAcousticAlert = true;   // the welder; footsteps stay silent to the visor
	Config->AnswerInterval = AnswerInterval;
	Config->AcousticAlertCooldown = AlertCooldown;
	Noise->SetConfig(Config);
	TArray<FHollowNoiseAnswer> Alerts;
	Noise->OnAcousticAlert.AddLambda([&Alerts](const FHollowNoiseAnswer& Answer) { Alerts.Add(Answer); });
	AActor* Pilgrim = World->SpawnActor<AActor>();
	AActor* Stalker = World->SpawnActor<AActor>();
	AActor* Welder = World->SpawnActor<AActor>();
	AActor* Other = World->SpawnActor<AActor>();
	auto Wait = [&TestWorld](const float Seconds)
	{
		for (float Elapsed = 0.f; Elapsed < Seconds; Elapsed += Step)
		{
			TestWorld.TickTestWorld(Step);
		}
	};

	TestFalse(TEXT("Footsteps: no answer"), Noise->AnswerNoise(*Pilgrim, Welder, HollowTags::Noise_Footstep));
	TestTrue(TEXT("Welding: the enemy answers"), Noise->AnswerNoise(*Pilgrim, Welder, HollowTags::Noise_Tool_Welder));
	TestTrue(TEXT("...and the welder's visor hears of it"), Alerts.Num() == 1 && Alerts[0].Instigator.Get() == Welder && Alerts[0].Enemy.Get() == Pilgrim
		&& Alerts[0].NoiseTag == HollowTags::Noise_Tool_Welder);
	TestFalse(TEXT("The same enemy again at once: no second answer"), Noise->AnswerNoise(*Pilgrim, Welder, HollowTags::Noise_Tool_Welder));
	TestTrue(TEXT("Another enemy answers"), Noise->AnswerNoise(*Stalker, Welder, HollowTags::Noise_Tool_Welder));
	TestEqual(TEXT("...but the welder's visor is told once per cooldown"), Alerts.Num(), 1);
	Wait(AnswerInterval + Step);
	TestTrue(TEXT("After its interval the enemy answers again"), Noise->AnswerNoise(*Pilgrim, Other, HollowTags::Noise_Tool_Welder));
	TestTrue(TEXT("...another player's noise: that player's visor"), Alerts.Num() == 2 && Alerts[1].Instigator.Get() == Other);
	Wait(AlertCooldown);
	TestTrue(TEXT("After the cooldown"), Noise->AnswerNoise(*Stalker, Welder, HollowTags::Noise_Tool_Welder));
	TestTrue(TEXT("...the welder's visor is told again"), Alerts.Num() == 3 && Alerts[2].Instigator.Get() == Welder);
	TestFalse(TEXT("A noise without a rule: no answer"), Noise->AnswerNoise(*Pilgrim, Welder, HollowTags::Noise_Voice));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowNoiseFootstepScaleTest, "Hollow.Core.Noise.FootstepScale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowNoiseFootstepScaleTest::RunTest(const FString& Parameters)
{
	using namespace HollowNoiseTests;
	const UHollowNoiseConfig* Config = MakeConfig();
	TestEqual(TEXT("Standing: silent"), Config->FootstepLoudnessScale(0.f), 0.f);
	TestEqual(TEXT("Crouch-walking: silent"), Config->FootstepLoudnessScale(Crouch), 0.f);
	TestEqual(TEXT("Walking: the rule's loudness"), Config->FootstepLoudnessScale(ReferenceSpeed), 1.f);
	TestEqual(TEXT("Sprinting: louder in proportion"), Config->FootstepLoudnessScale(Sprint), Sprint / ReferenceSpeed);
	TestTrue(TEXT("Just above the minimum: heard"), Config->FootstepLoudnessScale(MinSpeed) > 0.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
