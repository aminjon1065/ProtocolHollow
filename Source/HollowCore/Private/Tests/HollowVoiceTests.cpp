#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tags/HollowGameplayTags.h"
#include "Voice/HollowVoiceRules.h"

namespace HollowVoiceTests
{
	// Test data (the shipped numbers live in DefaultGame.ini, UHollowVoiceSettings).
	constexpr float FloorDb = -60.f;
	constexpr uint8 WhisperLevel = 60;
	constexpr uint8 SpeechLevel = 120;
	constexpr uint8 ShoutLevel = 200;
	constexpr int32 SampleCount = 480;
	constexpr int16 HalfScale = MAX_int16 / 2;

	FHollowVoiceTier Tier(const FGameplayTag& Tag, const uint8 MinLevel)
	{
		FHollowVoiceTier Result;
		Result.NoiseTag = Tag;
		Result.MinLevel = MinLevel;
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowVoiceRulesTest, "Hollow.Core.Voice.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::ProductFilter)

bool FHollowVoiceRulesTest::RunTest(const FString& Parameters)
{
	// Voice -> AI (S4-07, ADR-0025): microphone PCM -> one byte on a decibel scale -> a tier on the server; the client
	// sends only audible levels, no faster than the rate, a steady level again after a while; the server lets through
	// no more than its rate.
	using namespace HollowVoiceTests;

	// Loudness of PCM.
	TArray<int16> Samples;
	Samples.Init(0, SampleCount);
	TestEqual(TEXT("Silence: 0"), HollowVoiceRules::RmsAmplitude(Samples), 0.f);
	for (int32 Index = 0; Index < SampleCount; ++Index)
	{
		Samples[Index] = Index % 2 == 0 ? MAX_int16 : -MAX_int16;
	}
	TestTrue(TEXT("Full-scale square: 1"), FMath::IsNearlyEqual(HollowVoiceRules::RmsAmplitude(Samples), 1.f, KINDA_SMALL_NUMBER));
	for (int16& Sample : Samples)
	{
		Sample = Sample > 0 ? HalfScale : -HalfScale;
	}
	TestTrue(TEXT("Half-scale square: 0.5"), FMath::IsNearlyEqual(HollowVoiceRules::RmsAmplitude(Samples), 0.5f, 1.0e-3f));
	TestEqual(TEXT("No samples: 0"), HollowVoiceRules::RmsAmplitude(TConstArrayView<int16>()), 0.f);

	// Byte on a decibel scale.
	TestEqual(TEXT("0 dBFS: 255"), HollowVoiceRules::LevelFromAmplitude(1.f, FloorDb), static_cast<uint8>(MAX_uint8));
	TestEqual(TEXT("The floor: 0"), HollowVoiceRules::LevelFromAmplitude(FMath::Pow(10.f, FloorDb / 20.f), FloorDb), static_cast<uint8>(0));
	TestEqual(TEXT("Below the floor: 0"), HollowVoiceRules::LevelFromAmplitude(1.0e-6f, FloorDb), static_cast<uint8>(0));
	TestEqual(TEXT("Silence: 0"), HollowVoiceRules::LevelFromAmplitude(0.f, FloorDb), static_cast<uint8>(0));
	TestTrue(TEXT("-30 dBFS: half way"), FMath::Abs(HollowVoiceRules::LevelFromAmplitude(FMath::Pow(10.f, -30.f / 20.f), FloorDb) - 128) <= 1);
	TestTrue(TEXT("Louder is higher"), HollowVoiceRules::LevelFromAmplitude(0.5f, FloorDb) > HollowVoiceRules::LevelFromAmplitude(0.05f, FloorDb));
	TestEqual(TEXT("A floor of 0 dB measures nothing"), HollowVoiceRules::LevelFromAmplitude(1.f, 0.f), static_cast<uint8>(0));

	// Tiers (in any order).
	const TArray<FHollowVoiceTier> Tiers = {Tier(HollowTags::Noise_Voice_Shout, ShoutLevel), Tier(HollowTags::Noise_Voice_Whisper, WhisperLevel),
		Tier(HollowTags::Noise_Voice_Speech, SpeechLevel)};
	TestFalse(TEXT("Below the whisper: silence"), HollowVoiceRules::TierFor(WhisperLevel - 1, Tiers).IsValid());
	TestTrue(TEXT("Whisper"), HollowVoiceRules::TierFor(WhisperLevel, Tiers) == HollowTags::Noise_Voice_Whisper);
	TestTrue(TEXT("Speech"), HollowVoiceRules::TierFor(SpeechLevel + 10, Tiers) == HollowTags::Noise_Voice_Speech);
	TestTrue(TEXT("Shout"), HollowVoiceRules::TierFor(ShoutLevel, Tiers) == HollowTags::Noise_Voice_Shout);
	TestTrue(TEXT("Full scale: shout"), HollowVoiceRules::TierFor(MAX_uint8, Tiers) == HollowTags::Noise_Voice_Shout);
	TestEqual(TEXT("Quietest tier"), HollowVoiceRules::QuietestTierLevel(Tiers), static_cast<int32>(WhisperLevel));
	TestEqual(TEXT("No tiers: nothing is heard"), HollowVoiceRules::QuietestTierLevel({}), static_cast<int32>(MAX_uint8) + 1);
	TestFalse(TEXT("No tiers: no tier"), HollowVoiceRules::TierFor(MAX_uint8, {}).IsValid());

	// The system menu's cycle of modes.
	TestTrue(TEXT("Push-to-talk -> open mic -> muted -> push-to-talk"), HollowVoiceRules::NextMode(EHollowVoiceMode::PushToTalk) == EHollowVoiceMode::OpenMic
		&& HollowVoiceRules::NextMode(EHollowVoiceMode::OpenMic) == EHollowVoiceMode::Muted && HollowVoiceRules::NextMode(EHollowVoiceMode::Muted) == EHollowVoiceMode::PushToTalk);

	// Client throttle: 5 Hz at most, a steady level every 0.5 s, a change of 8 sooner, never silence.
	FHollowVoiceThrottle Throttle;
	Throttle.MinSeconds = 0.2f;
	Throttle.RepeatSeconds = 0.5f;
	Throttle.Delta = 8;
	Throttle.Gate = WhisperLevel;
	TestFalse(TEXT("Silence is not sent"), Throttle.ShouldSend(WhisperLevel - 1, 0.0));
	TestTrue(TEXT("The first audible level goes out"), Throttle.ShouldSend(SpeechLevel, 0.0));
	TestFalse(TEXT("Not faster than the rate, even a change"), Throttle.ShouldSend(ShoutLevel, 0.1));
	TestFalse(TEXT("A steady level waits"), Throttle.ShouldSend(SpeechLevel + 2, 0.3));
	TestTrue(TEXT("...a change goes out after the rate"), Throttle.ShouldSend(ShoutLevel, 0.3));
	TestTrue(TEXT("A steady level again after RepeatSeconds"), Throttle.ShouldSend(ShoutLevel, 0.8));
	Throttle.Reset();
	TestTrue(TEXT("After a reset: at once"), Throttle.ShouldSend(ShoutLevel, 0.85));

	// Server guard: 5 per second on average, 2 at once.
	FHollowVoiceRateGuard Guard;
	Guard.PerSecond = 5.f;
	Guard.Burst = 2.f;
	TestTrue(TEXT("First"), Guard.Admit(0.0));
	TestTrue(TEXT("A burst of two"), Guard.Admit(0.0));
	TestFalse(TEXT("A flood is cut"), Guard.Admit(0.01));
	TestTrue(TEXT("After 1/rate: one more"), Guard.Admit(0.22));
	int32 Admitted = 0;
	for (int32 Step = 0; Step < 100; ++Step)
	{
		Admitted += Guard.Admit(1.0 + Step * 0.01) ? 1 : 0;   // a second of 100 reports
	}
	TestTrue(FString::Printf(TEXT("A second of flood: about the rate plus the burst (%d)"), Admitted), Admitted >= 5 && Admitted <= 8);
	FHollowVoiceRateGuard Closed;
	TestFalse(TEXT("No rate: nothing"), Closed.Admit(0.0));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
