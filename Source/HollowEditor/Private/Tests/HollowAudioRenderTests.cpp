#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/IAssetRegistry.h"
#include "Audio.h"
#include "Interfaces/MetasoundFrontendSourceInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "MetasoundEnvironment.h"
#include "MetasoundFrontendRegistryContainer.h"
#include "MetasoundGraph.h"
#include "MetasoundGenerator.h"
#include "MetasoundOperatorSettings.h"
#include "MetasoundSource.h"

namespace HollowAudioRenderTests
{
	// Every MetaSound of the game (create_audio_assets.py): rendered offline, the way the mixer pulls them, without an
	// audio device (the tests run with -nosound).
	const TCHAR* const SoundsPath = TEXT("/Game/Hollow/Audio");
	/** The renders, for listening to them without starting the game (Saved/: not content, not in the package). */
	const TCHAR* const RenderFolder = TEXT("AudioRender");

	constexpr int32 SampleRate = 48000;
	/** One block of the MetaSound default block rate (100 per second). */
	constexpr int32 BlockFrames = 480;
	/** A one-shot still playing after this never ends: each play would leak a voice. */
	constexpr double MaxOneShotSeconds = 10.0;
	/** How much of a looping sound is rendered. */
	constexpr double LoopSeconds = 2.0;
	/** Full scale: a source above it clips before the mixer. */
	constexpr float MaxPeak = 1.f;
	/** -40 dBFS: quieter than this at its loudest is a silent (broken) graph. */
	constexpr float MinPeak = 0.01f;
	/** A sample at least this loud still sounds (the audible length ends at the last one). */
	constexpr float AudibleFloor = 1.e-4f;
	/** Noise nodes draw a new seed on every play: each sound is rendered this often and judged by its extremes. */
	constexpr int32 Renders = 16;

	/** UI-02 (ADR-0052): the menus' sounds (create_audio_assets.py UI_METASOUNDS); the focus tick is the quietest, by
	 * its peak and by its RMS. */
	const TCHAR* const MenuFocus = TEXT("MS_UI_Focus");
	const TCHAR* const MenuSounds[] = {TEXT("MS_UI_Focus"), TEXT("MS_UI_Confirm"), TEXT("MS_UI_Deny"), TEXT("MS_UI_Terminal")};
	/** Shorter than a quick double press, so the menus never pile their sounds up. */
	constexpr double MaxMenuSoundSeconds = 0.6;

	/** POLISH-01 (ADR-0053): an enemy's answer to a noise: a one-shot, heard at once and over before the hunt is on. */
	const TCHAR* const AlertSounds[] = {TEXT("MS_Enemy_PilgrimAlert"), TEXT("MS_Enemy_StalkerAlert")};
	constexpr double MaxAlertSeconds = 2.0;

	struct FRender
	{
		bool bBuilt = false;
		bool bFinished = false;
		int32 NumChannels = 0;
		TArray<float> Samples;   // interleaved
		double EndSeconds = 0.0;   // when a one-shot finished (to the end of its block), else the rendered length
		double AudibleSeconds = 0.0;
		float Peak = 0.f;
		float Rms = 0.f;
		int32 NonFinite = 0;
	};

	FRender Render(UMetaSoundSource& Source)
	{
		using namespace Metasound;
		using namespace Metasound::Frontend;
		FRender Result;
		// Registered as for a play (UMetaSoundSource::InitResources): the document's templates are resolved and the
		// graph the generator is built from is the registry's.
		Source.InitResources();
		const TSharedPtr<const Metasound::FGraph> Graph = FMetasoundFrontendRegistryContainer::Get()->GetGraph(Source.GetGraphRegistryKey());
		if (!Graph.IsValid())
		{
			return Result;
		}
		const FTopLevelAssetPath AssetPath(&Source);
		// The source's own environment (UMetaSoundSource::CreateEnvironment), one instance.
		const uint64 InstanceId = Source.GetUniqueID();
		FMetasoundEnvironment Environment;
		Environment.SetValue<uint32>(SourceInterface::Environment::SoundUniqueID, Source.GetUniqueID());
		Environment.SetValue<bool>(SourceInterface::Environment::IsPreview, false);
		Environment.SetValue<uint64>(CoreInterface::Environment::InstanceID, InstanceId);
		Environment.SetValue<uint64>(SourceInterface::Environment::TransmitterID, InstanceId);
		Environment.SetValue<int32>(SourceInterface::Environment::AudioMixerNumOutputFrames, BlockFrames);
		Environment.SetValue<float>(SourceInterface::Environment::AudioMixerSampleRate, static_cast<float>(SampleRate));
		FOperatorBuilderSettings BuilderSettings = FOperatorBuilderSettings::GetDefaultSettings();
		BuilderSettings.bFailOnAnyError = true;
		FGeneratorInitParams Params{
			.OperatorSettings = FOperatorSettings(SampleRate, static_cast<float>(SampleRate) / BlockFrames),
			.BuilderSettings = MoveTemp(BuilderSettings),
			.Graph = Graph,
			.Environment = Environment,
			.AudioOutputNames = Source.GetOutputAudioChannelOrder(),
			.bBuildSynchronous = true,
			.ClassName = Source.GetClass()->GetFName(),
			.AssetPath = AssetPath};
		FMetasoundConstGraphGenerator Generator(MoveTemp(Params));
		Result.NumChannels = Generator.GetNumChannels();
		if (Result.NumChannels < 1)
		{
			return Result;
		}
		Result.bBuilt = true;

		// The generator plays On Play itself. A one-shot's On Finished zeroes the rest of its block; the next call writes
		// nothing and reports the generator finished.
		const bool bOneShot = Source.IsOneShot();
		const int32 MaxFrames = FMath::CeilToInt32((bOneShot ? MaxOneShotSeconds : LoopSeconds) * SampleRate);
		TArray<float> Block;
		Block.SetNumZeroed(BlockFrames * Result.NumChannels);
		int32 Frames = 0;
		while (Frames < MaxFrames && !Generator.IsFinished())
		{
			const int32 Written = Generator.OnGenerateAudio(Block.GetData(), Block.Num());
			if (Written <= 0)
			{
				break;
			}
			Result.Samples.Append(Block.GetData(), Written);
			Frames += Written / Result.NumChannels;
		}
		Result.bFinished = Generator.IsFinished();
		Result.EndSeconds = static_cast<double>(Frames) / SampleRate;

		double SumSquares = 0.0;
		int32 LastAudible = INDEX_NONE;
		for (int32 Index = 0; Index < Result.Samples.Num(); ++Index)
		{
			const float Sample = Result.Samples[Index];
			if (!FMath::IsFinite(Sample))
			{
				++Result.NonFinite;
				continue;
			}
			Result.Peak = FMath::Max(Result.Peak, FMath::Abs(Sample));
			SumSquares += static_cast<double>(Sample) * Sample;
			if (FMath::Abs(Sample) >= AudibleFloor)
			{
				LastAudible = Index;
			}
		}
		const int32 AudibleSamples = LastAudible + 1;
		Result.AudibleSeconds = static_cast<double>(AudibleSamples / Result.NumChannels) / SampleRate;
		Result.Rms = AudibleSamples > 0 ? static_cast<float>(FMath::Sqrt(SumSquares / AudibleSamples)) : 0.f;
		return Result;
	}

	/** One sound over its renders. */
	struct FLevels
	{
		float LowestPeak = TNumericLimits<float>::Max();
		float HighestPeak = 0.f;
		float LowestRms = TNumericLimits<float>::Max();
		float HighestRms = 0.f;

		void Add(const FRender& Rendered)
		{
			LowestPeak = FMath::Min(LowestPeak, Rendered.Peak);
			HighestPeak = FMath::Max(HighestPeak, Rendered.Peak);
			LowestRms = FMath::Min(LowestRms, Rendered.Rms);
			HighestRms = FMath::Max(HighestRms, Rendered.Rms);
		}
	};

	float Decibels(float Amplitude)
	{
		return Amplitude > 0.f ? 20.f * FMath::LogX(10.f, Amplitude) : -200.f;
	}

	/** 16-bit PCM WAV of the render, for the ear. */
	bool WriteWave(const FRender& Rendered, const FString& Name)
	{
		TArray<int16> Pcm;
		Pcm.Reserve(Rendered.Samples.Num());
		for (const float Sample : Rendered.Samples)
		{
			const float Clamped = FMath::IsFinite(Sample) ? FMath::Clamp(Sample, -1.f, 1.f) : 0.f;
			Pcm.Add(static_cast<int16>(FMath::RoundToInt32(Clamped * MAX_int16)));
		}
		TArray<uint8> Wave;
		SerializeWaveFile(Wave, reinterpret_cast<const uint8*>(Pcm.GetData()), Pcm.Num() * sizeof(int16), Rendered.NumChannels, SampleRate);
		const FString File = FPaths::Combine(FPaths::ProjectSavedDir(), RenderFolder, Name + TEXT(".wav"));
		return FFileHelper::SaveArrayToFile(Wave, *File);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowAudioRenderTest, "Hollow.Editor.AudioRender",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowAudioRenderTest::RunTest(const FString& Parameters)
{
	// UI2-5 (ADR-0052): what can be checked of the sounds without an ear. Every MetaSound builds and sounds (no silent
	// graph), never clips or turns to NaN, and a one-shot ends (a voice is never leaked); the menus' sounds are short and
	// the focus tick, played on every step through a list, is the quietest of them. Each render is left in
	// Saved/AudioRender for the ear.
	using namespace HollowAudioRenderTests;
	IAssetRegistry& Registry = IAssetRegistry::GetChecked();
	Registry.ScanPathsSynchronous({FString(SoundsPath)}, /*bForceRescan=*/false);
	FARFilter Filter;
	Filter.PackagePaths.Add(FName(SoundsPath));
	Filter.bRecursivePaths = true;
	Filter.ClassPaths.Add(UMetaSoundSource::StaticClass()->GetClassPathName());
	TArray<FAssetData> Assets;
	Registry.GetAssets(Filter, Assets);
	Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.PackageName.LexicalLess(B.PackageName); });
	if (!TestTrue(TEXT("The game's MetaSounds are found"), Assets.Num() > 0))
	{
		return false;
	}

	TMap<FString, FLevels> MenuLevels;
	int32 AlertsRendered = 0;
	for (const FAssetData& Asset : Assets)
	{
		const FString Name = Asset.AssetName.ToString();
		UMetaSoundSource* Source = Cast<UMetaSoundSource>(Asset.GetAsset());
		if (!TestNotNull(*FString::Printf(TEXT("%s loads"), *Name), Source))
		{
			continue;
		}
		const FRender Rendered = Render(*Source);
		if (!TestTrue(*FString::Printf(TEXT("%s: its graph builds"), *Name), Rendered.bBuilt))
		{
			continue;
		}
		FLevels Levels;
		Levels.Add(Rendered);
		int32 NonFinite = Rendered.NonFinite;
		bool bAllFinished = Rendered.bFinished;
		bool bAnyFinished = Rendered.bFinished;
		for (int32 Again = 1; Again < Renders; ++Again)
		{
			const FRender Other = Render(*Source);
			Levels.Add(Other);
			NonFinite += Other.NonFinite;
			bAllFinished &= Other.bFinished;
			bAnyFinished |= Other.bFinished;
		}
		const bool bOneShot = Source->IsOneShot();
		AddInfo(FString::Printf(TEXT("%s: %s, %s %.2f s, sounds %.2f s, peak %.1f..%.1f dBFS, RMS %.1f..%.1f dBFS"), *Name,
			bOneShot ? TEXT("one-shot") : TEXT("loop"), bOneShot ? TEXT("ends at") : TEXT("rendered"), Rendered.EndSeconds,
			Rendered.AudibleSeconds, Decibels(Levels.LowestPeak), Decibels(Levels.HighestPeak), Decibels(Levels.LowestRms),
			Decibels(Levels.HighestRms)));
		TestEqual(*FString::Printf(TEXT("%s: no NaN or infinite sample"), *Name), NonFinite, 0);
		TestTrue(*FString::Printf(TEXT("%s: audible (peak %.1f dBFS)"), *Name, Decibels(Levels.LowestPeak)), Levels.LowestPeak >= MinPeak);
		TestTrue(*FString::Printf(TEXT("%s: does not clip (peak %.3f)"), *Name, Levels.HighestPeak), Levels.HighestPeak <= MaxPeak);
		if (bOneShot)
		{
			TestTrue(*FString::Printf(TEXT("%s: the one-shot ends within %.0f s"), *Name, MaxOneShotSeconds), bAllFinished);
		}
		else
		{
			TestFalse(*FString::Printf(TEXT("%s: the loop keeps playing"), *Name), bAnyFinished);
		}
		for (const TCHAR* Menu : MenuSounds)
		{
			if (Name == Menu)
			{
				MenuLevels.Add(Name, Levels);
				TestTrue(*FString::Printf(TEXT("%s: a menu sound is a one-shot"), *Name), bOneShot);
				TestTrue(*FString::Printf(TEXT("%s: a menu sound ends within %.1f s (%.2f s)"), *Name, MaxMenuSoundSeconds, Rendered.EndSeconds),
					Rendered.bFinished && Rendered.EndSeconds <= MaxMenuSoundSeconds);
			}
		}
		for (const TCHAR* AlertSound : AlertSounds)
		{
			if (Name == AlertSound)
			{
				++AlertsRendered;
				TestTrue(*FString::Printf(TEXT("%s: an acoustic alert is a one-shot within %.0f s (%.2f s)"), *Name, MaxAlertSeconds, Rendered.EndSeconds),
					bOneShot && bAllFinished && Rendered.EndSeconds <= MaxAlertSeconds);
			}
		}
		TestTrue(*FString::Printf(TEXT("%s: written to Saved/%s"), *Name, RenderFolder), WriteWave(Rendered, Name));
	}
	TestEqual(TEXT("Both acoustic alerts were rendered"), AlertsRendered, static_cast<int32>(UE_ARRAY_COUNT(AlertSounds)));

	TestEqual(TEXT("Every menu sound was rendered"), MenuLevels.Num(), static_cast<int32>(UE_ARRAY_COUNT(MenuSounds)));
	if (const FLevels* Focus = MenuLevels.Find(MenuFocus))
	{
		for (const TPair<FString, FLevels>& Menu : MenuLevels)
		{
			if (Menu.Key != MenuFocus)
			{
				TestTrue(*FString::Printf(TEXT("The focus tick peaks lower than %s"), *Menu.Key), Focus->HighestPeak < Menu.Value.LowestPeak);
				TestTrue(*FString::Printf(TEXT("The focus tick's RMS is lower than %s's"), *Menu.Key), Focus->HighestRms < Menu.Value.LowestRms);
			}
		}
	}
	return true;
}

#endif
