#include "Misc/AutomationTest.h"
#include "Perf/HollowFrameStats.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace HollowFrameStatsTests
{
	// Test data: E9's criteria; 100 frames, 95 of them at 15 ms and the rest slower; a mixer at 48 kHz with 1024-frame buffers.
	constexpr float TargetFps = 60.f;
	constexpr float Percentile = 95.f;
	constexpr float HitchMs = 50.f;
	constexpr float AudioBudgetMs = 1.5f;
	constexpr int32 Frames = 100;
	constexpr int32 FastFrames = 95;
	constexpr float FastMs = 15.f;
	constexpr float SlowMs = 30.f;
	constexpr float HitchFrameMs = 60.f;
	constexpr float SampleRate = 48000.f;
	constexpr int32 BufferFrames = 1024;
	constexpr float AudioMsPerBuffer = 1.f;
	constexpr float Tolerance = 0.01f;

	UHollowPerfSettings* Criteria()
	{
		UHollowPerfSettings* Settings = NewObject<UHollowPerfSettings>(GetTransientPackage());
		Settings->TargetFps = TargetFps;
		Settings->FramePercentile = Percentile;
		Settings->HitchMs = HitchMs;
		Settings->AudioBufferBudgetMs = AudioBudgetMs;
		return Settings;
	}

	TMap<FString, TArray<float>> FrameColumns(const int32 Fast, const float SlowFrameMs)
	{
		TMap<FString, TArray<float>> Columns;
		TArray<float>& Frame = Columns.Add(HollowFrameStats::Columns::Frame);
		for (int32 Index = 0; Index < Frames; ++Index)
		{
			Frame.Add(Index < Fast ? FastMs : SlowFrameMs);
		}
		Columns.Add(HollowFrameStats::Columns::Gpu, Frame);
		return Columns;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowFrameStatsTest, "Hollow.Core.Perf.FrameStats",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowFrameStatsTest::RunTest(const FString& Parameters)
{
	// The E9 verdict (ADR-0023): nearest-rank percentiles; a CSV profiler file read by its final header; an average of
	// frames over their time, the percentile within one target frame; hitches apart; audio per mixer buffer on any thread.
	using namespace HollowFrameStatsTests;
	TArray<float> OneToHundred;
	for (int32 Value = 1; Value <= Frames; ++Value)
	{
		OneToHundred.Add(static_cast<float>(Value));
	}
	TestEqual(TEXT("P95 of 1..100 (nearest rank)"), HollowFrameStats::PercentileOf(OneToHundred, Percentile), Percentile);
	TestEqual(TEXT("Median of 1..100"), HollowFrameStats::PercentileOf(OneToHundred, 50.f), 50.f);
	TestEqual(TEXT("Nothing: 0"), HollowFrameStats::PercentileOf({}, Percentile), 0.f);

	// The header written at the end knows a stat that appeared mid-capture; earlier rows are shorter.
	const FString Csv = TEXT("EVENTS,FrameTime\n,16.0\n,20.0,0.5\n,10.0,0.7\nEVENTS,FrameTime,Audio/AudioMixerRenderThread(1)/RenderAudio\n[HasHeaderRowAtEnd],1,[platform],Test\n");
	const TMap<FString, TArray<float>> Parsed = HollowFrameStats::ParseCsv(Csv);
	const TArray<float>* Frame = Parsed.Find(TEXT("FrameTime"));
	const TArray<float>* Audio = Parsed.Find(TEXT("Audio/AudioMixerRenderThread(1)/RenderAudio"));
	TestTrue(TEXT("CSV: three frames"), Frame && Frame->Num() == 3 && (*Frame)[1] == 20.f);
	TestTrue(TEXT("CSV: a stat from the final header, 0 where a row was short"), Audio && Audio->Num() == 3 && (*Audio)[0] == 0.f && (*Audio)[2] == 0.7f);
	TestFalse(TEXT("CSV: the metadata line is no frame"), Parsed.Contains(TEXT("[HasHeaderRowAtEnd]")));

	const UHollowPerfSettings* Settings = Criteria();
	const FHollowAudioClock NoAudio;
	const FHollowFrameReport Pass = HollowFrameStats::Evaluate(FrameColumns(FastFrames, SlowMs), *Settings, NoAudio);
	const double Seconds = (FastFrames * FastMs + (Frames - FastFrames) * SlowMs) / 1000.0;
	TestEqual(TEXT("Average FPS: frames over their time"), Pass.AverageFps, static_cast<float>(Frames / Seconds), Tolerance);
	TestEqual(TEXT("P95 frame"), Pass.Frame.Percentile, FastMs);
	TestTrue(TEXT("95 % within 16.67 ms and over 60 FPS: PASS"), Pass.bFramePass && Pass.Passed());
	TestEqual(TEXT("Share within target"), Pass.WithinTarget, static_cast<float>(FastFrames) / Frames, Tolerance);
	TestTrue(TEXT("The GPU timer is summarised too"), Pass.Gpu.Count == Frames && Pass.Gpu.Max == SlowMs);
	TestFalse(TEXT("One more slow frame: the P95 frame is slow, FAIL"), HollowFrameStats::Evaluate(FrameColumns(FastFrames - 1, SlowMs), *Settings, NoAudio).bFramePass);
	const FHollowFrameReport Hitchy = HollowFrameStats::Evaluate(FrameColumns(FastFrames, HitchFrameMs), *Settings, NoAudio);
	TestEqual(TEXT("Hitches over 50 ms are counted"), Hitchy.Hitches, Frames - FastFrames);
	TestFalse(TEXT("...and five of them pull the average under 60"), Hitchy.bFramePass);
	TestFalse(TEXT("No audio device: no audio verdict"), Pass.bHasAudio);

	FHollowAudioClock Clock;
	Clock.SampleRate = SampleRate;
	Clock.BufferFrames = BufferFrames;
	TMap<FString, TArray<float>> WithAudio = FrameColumns(FastFrames, SlowMs);
	const double Buffers = Seconds * SampleRate / BufferFrames;
	WithAudio.Add(TEXT("Audio/AudioMixerRenderThread(1)/RenderAudio"), {static_cast<float>(Buffers * AudioMsPerBuffer)});
	WithAudio.Add(TEXT("Audio_Metasound/AllWorkers/OnGenerateAudio"), {static_cast<float>(Buffers * AudioMsPerBuffer * 0.25)});
	WithAudio.Add(TEXT("Audio_Metasound/AudioMixerRenderThread(1)/OnGenerateAudio"), {static_cast<float>(Buffers * AudioMsPerBuffer * 0.25)});
	WithAudio.Add(TEXT("Audio/AudioMixerRenderThread(1)/SourceBuffers"), {1000.f});   // another audio timer: not the render
	const FHollowFrameReport Sound = HollowFrameStats::Evaluate(WithAudio, *Settings, Clock);
	TestTrue(TEXT("Audio: render time per buffer"), Sound.bHasAudio && FMath::IsNearlyEqual(Sound.AudioMsPerBuffer, AudioMsPerBuffer, Tolerance));
	TestEqual(TEXT("...MetaSounds summed over the render thread and the workers"), Sound.MetaSoundMsPerBuffer, AudioMsPerBuffer * 0.5f, Tolerance);
	TestTrue(TEXT("...within 1.5 ms: PASS"), Sound.bAudioPass && Sound.Passed());
	TestTrue(TEXT("The summary names the verdict"), Sound.Describe(*Settings).StartsWith(TEXT("PASS")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
