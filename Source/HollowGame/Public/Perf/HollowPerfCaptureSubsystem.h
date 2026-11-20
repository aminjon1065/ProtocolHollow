#pragma once

#include "Containers/Ticker.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"

#include "HollowPerfCaptureSubsystem.generated.h"

class UAudioComponent;
struct FHollowFrameReport;

/**
 * Performance capture of a running game (ADR-0023, E9), in every build but Shipping. A capture waits WarmupSeconds,
 * records the CSV profiler for Seconds (frame, game, render, RHI and GPU times, audio and MetaSound render times) between
 * two Unreal Insights bookmarks, then judges it against UHollowPerfSettings: one log line "Hollow.Perf: <Name> PASS|FAIL ..."
 * and a JSON report next to the CSV (Saved/Profiling/Hollow). Also the camera sweep and the audio stress of the
 * benchmark scenario. Nothing runs between captures.
 */
UCLASS()
class HOLLOWGAME_API UHollowPerfCaptureSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UHollowPerfCaptureSubsystem* Get(const UObject* WorldContext);

	/** False if a capture is running or this build has no CSV profiler. */
	bool StartCapture(const FString& Name, float Seconds, float WarmupSeconds);
	bool IsCapturing() const { return !CaptureName.IsEmpty(); }

	/** Turns the local player's view Turns times round over Seconds, evenly (a repeatable view for captures). */
	bool StartLookAround(float Seconds, float Turns);

	/** Every bank sound Copies times at the local player for Seconds: loops held, one-shots replayed (UHollowPerfSettings). */
	bool StartAudioStress(int32 Copies, float Seconds);

	/** The folder of the reports. */
	static FString GetReportFolder();

protected:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;

private:
	void BeginRecording();
	void EndRecording();
	void WaitForFile();
	void Report(const FString& CsvPath);
	void WriteJson(const FString& CsvPath, const FHollowFrameReport& Result) const;

	bool StepLookAround();
	void ReplayOneShots();
	void EndAudioStress();

	FString CaptureName;
	float CaptureSeconds = 0.f;
	FString CsvBaseName;
	FTimerHandle CaptureTimer;
	TSharedPtr<TSharedFuture<FString>> PendingFile;

	FTSTicker::FDelegateHandle LookTicker;
	double LookStart = 0.0;
	float LookSeconds = 0.f;
	float LookTurns = 0.f;
	FRotator LookFrom = FRotator::ZeroRotator;

	FTimerHandle StressTimer;
	FTimerHandle StressEndTimer;
	int32 StressCopies = 0;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> StressLoops;
};
