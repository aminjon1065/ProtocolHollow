#include "Tests/HollowGameTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/CameraComponent.h"
#include "Framework/HollowPlayerController.h"
#include "Framework/HollowPlayerState.h"
#include "Noise/HollowNoiseConfig.h"
#include "Noise/HollowNoiseSubsystem.h"
#include "Settings/HollowGameUserSettings.h"
#include "Tags/HollowGameplayTags.h"
#include "Voice/HollowVoiceComponent.h"
#include "Voice/HollowVoiceConfig.h"
#include "Voice/HollowVoiceSettings.h"
#include "Voice/HollowVoiceTalker.h"

namespace HollowVoiceGameTests
{
	constexpr float TickSeconds = 1.f / 60.f;
	constexpr double LocationTolerance = 1.0;
	constexpr float SimulateSeconds = 1.f;
	constexpr float SimulateSlack = 0.5f;

	void Advance(FTestWorldWrapper& World, const float Seconds)
	{
		for (float Elapsed = 0.f; Elapsed < Seconds; Elapsed += TickSeconds)
		{
			++GFrameCounter;   // the timer manager ticks once per engine frame
			World.TickTestWorld(TickSeconds);
		}
	}

	/** The level where the tier of Tag begins (DefaultGame.ini), or -1. */
	int32 LevelOf(const UHollowVoiceSettings& Settings, const FGameplayTag& Tag)
	{
		const FHollowVoiceTier* Tier = Settings.Tiers.FindByPredicate([&Tag](const FHollowVoiceTier& T) { return T.NoiseTag == Tag; });
		return Tier ? Tier->MinLevel : INDEX_NONE;
	}

	int32 CountVoice(const TArray<FHollowNoiseReport>& Reports, const FGameplayTag& Tag)
	{
		return Reports.FilterByPredicate([&Tag](const FHollowNoiseReport& R) { return R.Event.NoiseTag == Tag; }).Num();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowVoiceReportTest, "Hollow.Game.Voice.Report",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowVoiceReportTest::RunTest(const FString& Parameters)
{
	// S4-07 (ADR-0025): one byte from the client becomes the noise of its tier at a living ExoFrame's head; silence,
	// floods, the dead and the bodiless are not heard; a simulated microphone takes the same way; the mode decides
	// whether the voice goes out.
	using namespace HollowVoiceGameTests;
	const UHollowVoiceSettings* Settings = GetDefault<UHollowVoiceSettings>();
	const int32 Whisper = LevelOf(*Settings, HollowTags::Noise_Voice_Whisper);
	const int32 Speech = LevelOf(*Settings, HollowTags::Noise_Voice_Speech);
	const int32 Shout = LevelOf(*Settings, HollowTags::Noise_Voice_Shout);
	FTestWorldWrapper TestWorld;
	UWorld* World = HollowGameTests::CreateWorld(*this, TestWorld);
	AHollowExoFrameCharacter* Character = HollowGameTests::SpawnExoFrame(*this, World);
	UHollowNoiseSubsystem* Noise = UHollowNoiseSubsystem::Get(World);
	if (!TestTrue(TEXT("Three tiers in DefaultGame.ini, quiet to loud"), Whisper > 0 && Speech > Whisper && Shout > Speech)
		|| !TestTrue(TEXT("Report rate configured"), Settings->MaxReportsPerSecond > 0.f && Settings->ServerBurst >= 1.f) || !Character
		|| !TestNotNull(TEXT("Noise subsystem"), Noise) || !TestNotNull(TEXT("DA_NoiseConfig"), Noise->GetConfig()))
	{
		return false;
	}
	for (const FGameplayTag& Tag : TArray<FGameplayTag>{HollowTags::Noise_Voice_Whisper, HollowTags::Noise_Voice_Speech, HollowTags::Noise_Voice_Shout})
	{
		TestNotNull(*FString::Printf(TEXT("%s has a rule in DA_NoiseConfig"), *Tag.ToString()), Noise->GetConfig()->FindRule(Tag));
	}
	AddExpectedMessage(TEXT("InputConfig is not set"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, /*Occurrences=*/0, /*IsRegex=*/false);
	AHollowPlayerController* Controller = HollowGameTests::PossessAsPlayer<AHollowPlayerController>(World, Character, /*bLocalPlayer=*/true);
	Character->DispatchBeginPlay();
	Controller->DispatchBeginPlay();
	UHollowVoiceComponent* Voice = Controller->GetVoice();
	if (!TestNotNull(TEXT("The player controller has a voice"), Voice))
	{
		return false;
	}
	TArray<FHollowNoiseReport> Reports;
	Noise->OnNoiseReported.AddLambda([&Reports](const FHollowNoiseReport& Report)
	{
		if (Report.Event.NoiseTag.MatchesTag(HollowTags::Noise_Voice))
		{
			Reports.Add(Report);
		}
	});
	const float Interval = 1.f / Settings->MaxReportsPerSecond + Noise->GetConfig()->FindRule(HollowTags::Noise_Voice_Shout)->MinInterval;

	// Each tier, at the head, by the body.
	for (const TPair<FGameplayTag, int32>& Case : TArray<TPair<FGameplayTag, int32>>{{HollowTags::Noise_Voice_Whisper, Whisper},
		{HollowTags::Noise_Voice_Speech, Speech}, {HollowTags::Noise_Voice_Shout, Shout}})
	{
		Advance(TestWorld, Interval);
		TestTrue(*FString::Printf(TEXT("Level %d is heard"), Case.Value), Voice->HandleReport(static_cast<uint8>(Case.Value)));
		TestTrue(*FString::Printf(TEXT("...as %s, at the head, by the body"), *Case.Key.ToString()), !Reports.IsEmpty() && Reports.Last().Event.NoiseTag == Case.Key
			&& Reports.Last().Event.Instigator.Get() == Character && FVector::Dist(Reports.Last().Event.Location, Character->GetPawnViewLocation()) < LocationTolerance);
	}
	Advance(TestWorld, Interval);
	const int32 Before = Reports.Num();
	TestFalse(TEXT("Below the whisper: silence"), Voice->HandleReport(static_cast<uint8>(Whisper - 1)));
	TestEqual(TEXT("...no noise"), Reports.Num(), Before);

	// A flood: the burst gets through (two different tiers, so the noise rules' own rate does not interfere), no more.
	Advance(TestWorld, Interval);
	const int32 FloodBefore = Reports.Num();
	Voice->HandleReport(static_cast<uint8>(Whisper));
	Voice->HandleReport(static_cast<uint8>(Speech));
	TestFalse(TEXT("A third report at once is dropped"), Voice->HandleReport(static_cast<uint8>(Shout)));
	TestEqual(TEXT("...two noises"), Reports.Num(), FloodBefore + FMath::FloorToInt(Settings->ServerBurst));

	// A simulated microphone: through the timer, the throttle and the RPC to the same server path.
	Advance(TestWorld, Interval);
	const int32 ShoutsBefore = CountVoice(Reports, HollowTags::Noise_Voice_Shout);
	Voice->Simulate(static_cast<uint8>(Shout), SimulateSeconds);
	TestTrue(TEXT("Simulating: transmitting"), Voice->IsTransmitting());
	Advance(TestWorld, SimulateSeconds * 0.5f);
	TestEqual(TEXT("...at the simulated level"), static_cast<int32>(Voice->GetLevel()), Shout);
	Advance(TestWorld, SimulateSeconds * 0.5f + SimulateSlack);
	TestTrue(TEXT("...heard as a shout"), CountVoice(Reports, HollowTags::Noise_Voice_Shout) > ShoutsBefore);
	TestTrue(TEXT("The simulation ends: silent"), !Voice->IsTransmitting() && Voice->GetLevel() == 0);

	// The mode: muted never goes out; push-to-talk while held. Restored for the editor's settings.
	const EHollowVoiceMode Original = Voice->GetMode();
	Voice->SetMode(EHollowVoiceMode::Muted);
	Voice->SetPushToTalk(true);
	TestFalse(TEXT("Muted: the key does nothing"), Voice->IsTransmitting());
	Voice->SetMode(EHollowVoiceMode::PushToTalk);
	TestTrue(TEXT("Push-to-talk held: transmitting"), Voice->IsTransmitting());
	Voice->SetPushToTalk(false);
	TestFalse(TEXT("...released: not"), Voice->IsTransmitting());
	Voice->SetMode(EHollowVoiceMode::OpenMic);
	TestTrue(TEXT("Open mic: transmitting"), Voice->IsTransmitting());
	TestTrue(TEXT("The mode is kept in the player's settings"), !UHollowGameUserSettings::Get() || UHollowGameUserSettings::Get()->GetVoiceMode() == EHollowVoiceMode::OpenMic);
	Voice->SetMode(Original);

	// The dead and the bodiless are not heard.
	Advance(TestWorld, Interval);
	Character->Die();
	TestFalse(TEXT("The dead are not heard"), Voice->HandleReport(static_cast<uint8>(Shout)));
	Controller->UnPossess();
	Advance(TestWorld, Interval);
	TestFalse(TEXT("Nor is a player without a body"), Voice->HandleReport(static_cast<uint8>(Shout)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowVoiceTalkerTest, "Hollow.Game.Voice.Talker",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowVoiceTalkerTest::RunTest(const FString& Parameters)
{
	// S4-07 (ADR-0025): a player's voice comes from the helmet of its living body (3D, attenuation, helmet filter) and
	// goes on the radio (2D, radio filter) when the body dies or the player has none; DA_VoiceConfig is complete.
	using namespace HollowVoiceGameTests;
	const UHollowVoiceConfig* Config = GetDefault<UHollowVoiceSettings>()->LoadConfig();
	if (!TestNotNull(TEXT("DA_VoiceConfig"), Config) || !TestTrue(TEXT("...complete"), Config->PushToTalkAction && Config->MappingContext
		&& Config->Attenuation && Config->HelmetChain && Config->RadioChain && Config->HelmetChain != Config->RadioChain))
	{
		return false;
	}
	FTestWorldWrapper TestWorld;
	UWorld* World = HollowGameTests::CreateWorld(*this, TestWorld);
	AHollowExoFrameCharacter* First = HollowGameTests::SpawnExoFrame(*this, World);
	AHollowExoFrameCharacter* Second = HollowGameTests::SpawnExoFrame(*this, World, FTransform(FVector(500.0, 0.0, 0.0)));
	if (!First || !Second)
	{
		return false;
	}
	AHollowPlayerController* Controller = HollowGameTests::PossessAsPlayer<AHollowPlayerController>(World, First);
	AHollowPlayerState* State = Controller->GetPlayerState<AHollowPlayerState>();
	UHollowVoiceTalker* Talker = State ? State->GetVoiceTalker() : nullptr;
	if (!TestNotNull(TEXT("The player state has a talker"), Talker))
	{
		return false;
	}
	State->DispatchBeginPlay();   // the talker follows the player state's pawn from now on
	auto OnHelmet = [Talker, Config](const AHollowExoFrameCharacter* Body)
	{
		return !Talker->IsOnRadio() && Talker->Settings.ComponentToAttachTo == Body->GetFirstPersonCamera()
			&& Talker->Settings.AttenuationSettings == Config->Attenuation && Talker->Settings.SourceEffectChain == Config->HelmetChain;
	};
	auto OnRadio = [Talker, Config]
	{
		return Talker->IsOnRadio() && !Talker->Settings.ComponentToAttachTo && !Talker->Settings.AttenuationSettings
			&& Talker->Settings.SourceEffectChain == Config->RadioChain;
	};
	TestTrue(TEXT("Alive: from the helmet"), OnHelmet(First));
	TestFalse(TEXT("No unique net id in a test world: not registered with the voice backend"), Talker->IsRegisteredForVoice());
	First->Die();
	TestTrue(TEXT("Dead: on the radio"), OnRadio());
	Controller->UnPossess();
	Controller->Possess(Second);
	TestTrue(TEXT("A new body: from its helmet"), OnHelmet(Second));
	Controller->UnPossess();
	TestTrue(TEXT("No body: on the radio"), OnRadio());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
