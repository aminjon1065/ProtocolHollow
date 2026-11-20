#pragma once

#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Engine/DeveloperSettings.h"

#include "HollowFrameStats.generated.h"

/**
 * What a performance capture must meet (ADR-0023): E9 is an average of at least TargetFps with FramePercentile of the
 * frames within one target frame; audio rendering within its budget per mixer buffer. Data in DefaultGame.ini.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Hollow Performance"))
class HOLLOWCORE_API UHollowPerfSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Criteria", meta = (ClampMin = "1"))
	float TargetFps = 0.f;

	/** This share of the frames (percent) must fit in one target frame. */
	UPROPERTY(Config, EditAnywhere, Category = "Criteria", meta = (ClampMin = "1", ClampMax = "100"))
	float FramePercentile = 0.f;

	/** A frame longer than this is a hitch (reported, not part of the verdict). */
	UPROPERTY(Config, EditAnywhere, Category = "Criteria", meta = (ClampMin = "1", Units = "Milliseconds"))
	float HitchMs = 0.f;

	/** Audio render time allowed per mixer buffer (MetaSounds included). */
	UPROPERTY(Config, EditAnywhere, Category = "Criteria", meta = (ClampMin = "0.01", Units = "Milliseconds"))
	float AudioBufferBudgetMs = 0.f;

	/** Defaults of Hollow.Perf.Capture: settle first, then measure. */
	UPROPERTY(Config, EditAnywhere, Category = "Capture", meta = (ClampMin = "0", Units = "Seconds"))
	float WarmupSeconds = 0.f;

	UPROPERTY(Config, EditAnywhere, Category = "Capture", meta = (ClampMin = "1", Units = "Seconds"))
	float CaptureSeconds = 0.f;

	/** CSV categories a capture turns on (the audio and MetaSound render times). */
	UPROPERTY(Config, EditAnywhere, Category = "Capture")
	TArray<FString> CsvCategoriesOn;

	/** CSV categories a capture turns off because recording them costs frame time (VSM: a GPU stats pass every frame). */
	UPROPERTY(Config, EditAnywhere, Category = "Capture")
	TArray<FString> CsvCategoriesOff;

	/** Hollow.Audio.Stress: one-shots are played again this often. */
	UPROPERTY(Config, EditAnywhere, Category = "Capture", meta = (ClampMin = "0.01", Units = "Seconds"))
	float AudioStressRetriggerSeconds = 0.f;

	float TargetFrameMs() const { return TargetFps > 0.f ? 1000.f / TargetFps : 0.f; }
};

/** Average, median, the criteria's percentile and maximum of one timer over a capture (ms). */
struct HOLLOWCORE_API FHollowTimingSummary
{
	int32 Count = 0;
	float Average = 0.f;
	float Median = 0.f;
	float Percentile = 0.f;
	float Max = 0.f;
};

/** The verdict of one capture. */
struct HOLLOWCORE_API FHollowFrameReport
{
	int32 Frames = 0;
	double Seconds = 0.0;
	FHollowTimingSummary Frame;
	FHollowTimingSummary GameThread;
	FHollowTimingSummary RenderThread;
	FHollowTimingSummary RhiThread;
	FHollowTimingSummary Gpu;
	/** Frames over the time they took (not a mean of instantaneous rates). */
	float AverageFps = 0.f;
	/** Share of frames within one target frame (0..1). */
	float WithinTarget = 0.f;
	int32 Hitches = 0;
	bool bFramePass = false;

	/** Audio rendering (zero without an audio device: no verdict then). */
	bool bHasAudio = false;
	float AudioMsPerBuffer = 0.f;
	float MetaSoundMsPerBuffer = 0.f;
	/** Share of the time the audio render thread was busy (0..1). */
	float AudioLoad = 0.f;
	bool bAudioPass = true;

	bool Passed() const { return bFramePass && bAudioPass; }
	FString Describe(const UHollowPerfSettings& Criteria) const;
};

/** The mixer's pace during a capture: buffers it rendered per second. */
struct HOLLOWCORE_API FHollowAudioClock
{
	float SampleRate = 0.f;
	int32 BufferFrames = 0;

	bool IsValid() const { return SampleRate > 0.f && BufferFrames > 0; }
	double BuffersPerSecond() const { return IsValid() ? SampleRate / BufferFrames : 0.0; }
};

/** Pure statistics of a capture (CSV profiler file, ADR-0023). */
namespace HollowFrameStats
{
	/** CSV profiler columns by name. Uses the header at the end when the file has one (stats added mid-capture). */
	HOLLOWCORE_API TMap<FString, TArray<float>> ParseCsv(const FString& Text);

	/** Nearest-rank percentile (0..100) of Values; 0 for none. */
	HOLLOWCORE_API float PercentileOf(TConstArrayView<float> Values, float Percent);

	HOLLOWCORE_API FHollowTimingSummary Summarize(TConstArrayView<float> Values, float Percent);

	/** Columns read: FrameTime, GameThreadTime, RenderThreadTime, RHIThreadTime, GPUTime; Audio/<thread>/RenderAudio and Audio_Metasound/<thread>/OnGenerateAudio on any thread. */
	HOLLOWCORE_API FHollowFrameReport Evaluate(const TMap<FString, TArray<float>>& Columns, const UHollowPerfSettings& Criteria, const FHollowAudioClock& Audio);

	namespace Columns
	{
		inline const TCHAR* Frame = TEXT("FrameTime");
		inline const TCHAR* GameThread = TEXT("GameThreadTime");
		inline const TCHAR* RenderThread = TEXT("RenderThreadTime");
		inline const TCHAR* RhiThread = TEXT("RHIThreadTime");
		inline const TCHAR* Gpu = TEXT("GPUTime");
		/** Audio timings come from the mixer's render thread and task workers: summed over every thread. */
		inline const TCHAR* AudioCategory = TEXT("Audio");
		inline const TCHAR* AudioRender = TEXT("RenderAudio");
		inline const TCHAR* MetaSoundCategory = TEXT("Audio_Metasound");
		inline const TCHAR* MetaSound = TEXT("OnGenerateAudio");
	}
}
