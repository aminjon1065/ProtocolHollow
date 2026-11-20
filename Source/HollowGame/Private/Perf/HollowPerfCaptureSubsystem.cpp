#include "Perf/HollowPerfCaptureSubsystem.h"

#include "Audio/HollowAudioSubsystem.h"
#include "Audio/HollowSoundBank.h"
#include "AudioDevice.h"
#include "Components/AudioComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GenericPlatform/GenericPlatformDriver.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HollowGameLog.h"
#include "HAL/PlatformMisc.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Perf/HollowFrameStats.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "RHI.h"
#include "RHIStats.h"
#include "Scalability.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Settings/HollowGameUserSettings.h"
#include "Sound/SoundBase.h"
#include "UnrealClient.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowPerfCaptureSubsystem)

namespace HollowPerfCapturePrivate
{
	const TCHAR* const ReportFolderName = TEXT("Hollow");
	constexpr double FullTurnDegrees = 360.0;
	constexpr double BytesPerMegabyte = 1024.0 * 1024.0;

	TSharedRef<FJsonObject> Timing(const FHollowTimingSummary& Summary)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetNumberField(TEXT("average"), Summary.Average);
		Object->SetNumberField(TEXT("median"), Summary.Median);
		Object->SetNumberField(TEXT("percentile"), Summary.Percentile);
		Object->SetNumberField(TEXT("max"), Summary.Max);
		return Object;
	}

	APlayerController* LocalPlayer(const UWorld* World)
	{
		return World ? World->GetFirstPlayerController() : nullptr;
	}
}

UHollowPerfCaptureSubsystem* UHollowPerfCaptureSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UHollowPerfCaptureSubsystem>() : nullptr;
}

bool UHollowPerfCaptureSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return Super::ShouldCreateSubsystem(Outer);
#endif
}

bool UHollowPerfCaptureSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UHollowPerfCaptureSubsystem::Deinitialize()
{
	FTSTicker::RemoveTicker(LookTicker);
	LookTicker.Reset();
#if CSV_PROFILER
	if (IsCapturing() && !PendingFile.IsValid() && FCsvProfiler::Get()->IsCapturing())
	{
		FCsvProfiler::Get()->EndCapture();   // the world goes mid-capture: close the file, no verdict
		UE_LOG(LogHollowGame, Warning, TEXT("Hollow.Perf: %s cut short by the end of the world."), *CaptureName);
	}
#endif
	Super::Deinitialize();
}

FString UHollowPerfCaptureSubsystem::GetReportFolder()
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProfilingDir(), HollowPerfCapturePrivate::ReportFolderName));
}

bool UHollowPerfCaptureSubsystem::StartCapture(const FString& Name, const float Seconds, const float WarmupSeconds)
{
#if CSV_PROFILER
	if (IsCapturing() || Name.IsEmpty() || Seconds <= 0.f || FCsvProfiler::Get()->IsCapturing())
	{
		UE_LOG(LogHollowGame, Warning, TEXT("Hollow.Perf: cannot start %s (a capture is running, or no name or length)."), *Name);
		return false;
	}
	CaptureName = Name;
	CaptureSeconds = Seconds;
	UE_LOG(LogHollowGame, Display, TEXT("Hollow.Perf: %s in %.0f s, for %.0f s"), *Name, WarmupSeconds, Seconds);
	if (WarmupSeconds > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(CaptureTimer, this, &ThisClass::BeginRecording, WarmupSeconds, /*bLoop=*/false);
	}
	else
	{
		BeginRecording();
	}
	return true;
#else
	UE_LOG(LogHollowGame, Warning, TEXT("Hollow.Perf: this build has no CSV profiler."));
	return false;
#endif
}

void UHollowPerfCaptureSubsystem::BeginRecording()
{
#if CSV_PROFILER
	FCsvProfiler* Csv = FCsvProfiler::Get();
	const UHollowPerfSettings* Settings = GetDefault<UHollowPerfSettings>();
	for (const FString& Category : Settings->CsvCategoriesOn)
	{
		UE_CLOG(!Csv->EnableCategoryByString(Category), LogHollowGame, Warning, TEXT("Hollow.Perf: no CSV category %s in this build."), *Category);
	}
	for (const FString& Category : Settings->CsvCategoriesOff)
	{
		const int32 Index = FCsvProfiler::GetCategoryIndex(Category);
		if (Index >= 0)
		{
			Csv->EnableCategoryByIndex(Index, false);   // left off: a later capture wants the same conditions
		}
	}
	CsvBaseName = FString::Printf(TEXT("%s-%s"), *CaptureName, *FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
	Csv->BeginCapture(-1, GetReportFolder(), CsvBaseName + TEXT(".csv"));
	TRACE_BOOKMARK(TEXT("Hollow.Perf %s begin"), *CaptureName);
	UE_LOG(LogHollowGame, Display, TEXT("Hollow.Perf: %s recording"), *CaptureName);
	GetWorld()->GetTimerManager().SetTimer(CaptureTimer, this, &ThisClass::EndRecording, CaptureSeconds, /*bLoop=*/false);
#endif
}

void UHollowPerfCaptureSubsystem::EndRecording()
{
#if CSV_PROFILER
	TRACE_BOOKMARK(TEXT("Hollow.Perf %s end"), *CaptureName);
	PendingFile = MakeShared<TSharedFuture<FString>>(FCsvProfiler::Get()->EndCapture());
	WaitForFile();
#endif
}

void UHollowPerfCaptureSubsystem::WaitForFile()
{
	// The CSV is written on another thread; look again next frame until it is on disk.
	if (!PendingFile.IsValid() || !PendingFile->IsReady())
	{
		GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::WaitForFile);
		return;
	}
	const FString CsvPath = PendingFile->Get();
	PendingFile.Reset();
	Report(CsvPath);
	CaptureName.Reset();
}

void UHollowPerfCaptureSubsystem::Report(const FString& CsvPath)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *CsvPath))
	{
		UE_LOG(LogHollowGame, Error, TEXT("Hollow.Perf: %s: cannot read %s"), *CaptureName, *CsvPath);
		return;
	}
	FHollowAudioClock Clock;
	if (const FAudioDeviceHandle Device = GetWorld()->GetAudioDevice())
	{
		Clock.SampleRate = Device->GetSampleRate();
		Clock.BufferFrames = Device->GetBufferLength();
	}
	const UHollowPerfSettings* Criteria = GetDefault<UHollowPerfSettings>();
	const FHollowFrameReport Result = HollowFrameStats::Evaluate(HollowFrameStats::ParseCsv(Text), *Criteria, Clock);
	WriteJson(CsvPath, Result);
	UE_LOG(LogHollowGame, Display, TEXT("Hollow.Perf: %s %s"), *CaptureName, *Result.Describe(*Criteria));
}

void UHollowPerfCaptureSubsystem::WriteJson(const FString& CsvPath, const FHollowFrameReport& Result) const
{
	using namespace HollowPerfCapturePrivate;
	const UWorld* World = GetWorld();
	const UHollowPerfSettings* Criteria = GetDefault<UHollowPerfSettings>();
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("name"), CaptureName);
	Root->SetStringField(TEXT("verdict"), Result.Passed() ? TEXT("PASS") : TEXT("FAIL"));
	Root->SetBoolField(TEXT("framePass"), Result.bFramePass);
	Root->SetBoolField(TEXT("audioPass"), Result.bAudioPass);
	Root->SetStringField(TEXT("summary"), Result.Describe(*Criteria));
	Root->SetStringField(TEXT("csv"), CsvPath);

	TSharedRef<FJsonObject> Machine = MakeShared<FJsonObject>();
	Machine->SetStringField(TEXT("platform"), FPlatformProperties::IniPlatformName());
	Machine->SetStringField(TEXT("build"), LexToString(FApp::GetBuildConfiguration()));
	Machine->SetStringField(TEXT("rhi"), GDynamicRHI ? GDynamicRHI->GetName() : TEXT("none"));
	Machine->SetStringField(TEXT("gpu"), GRHIAdapterName);
	Machine->SetStringField(TEXT("gpuDriver"), FPlatformMisc::GetGPUDriverInfo(GRHIAdapterName).UserDriverVersion);
	FTextureMemoryStats Memory;
	RHIGetTextureMemoryStats(Memory);
	Machine->SetNumberField(TEXT("vramMB"), static_cast<double>(Memory.DedicatedVideoMemory) / BytesPerMegabyte);
	Machine->SetStringField(TEXT("cpu"), FPlatformMisc::GetCPUBrand().TrimStartAndEnd());
	Machine->SetNumberField(TEXT("cores"), FPlatformMisc::NumberOfCoresIncludingHyperthreads());
	Machine->SetNumberField(TEXT("ramGB"), FPlatformMemory::GetConstants().TotalPhysicalGB);
	Root->SetObjectField(TEXT("machine"), Machine);

	TSharedRef<FJsonObject> Scene = MakeShared<FJsonObject>();
	const UHollowGameUserSettings* Settings = UHollowGameUserSettings::Get();
	Scene->SetStringField(TEXT("preset"), Settings ? UHollowGameUserSettings::PresetName(Settings->GetPreset()) : TEXT("unknown"));
	const Scalability::FQualityLevels Levels = Scalability::GetQualityLevels();
	Scene->SetNumberField(TEXT("shadowQuality"), Levels.ShadowQuality);
	Scene->SetNumberField(TEXT("giQuality"), Levels.GlobalIlluminationQuality);
	Scene->SetNumberField(TEXT("screenPercentage"), Levels.ResolutionQuality);
	const FViewport* Viewport = GEngine && GEngine->GameViewport ? GEngine->GameViewport->Viewport : nullptr;
	Scene->SetStringField(TEXT("resolution"), Viewport ? FString::Printf(TEXT("%dx%d"), Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y) : TEXT("none"));
	Scene->SetStringField(TEXT("map"), World ? World->GetMapName() : TEXT("none"));
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	Scene->SetNumberField(TEXT("players"), GameState ? GameState->PlayerArray.Num() : 0);
	Scene->SetStringField(TEXT("netMode"), World && World->GetNetMode() == NM_Client ? TEXT("client") : TEXT("host"));
	Root->SetObjectField(TEXT("scene"), Scene);

	TSharedRef<FJsonObject> Frames = MakeShared<FJsonObject>();
	Frames->SetNumberField(TEXT("frames"), Result.Frames);
	Frames->SetNumberField(TEXT("seconds"), Result.Seconds);
	Frames->SetNumberField(TEXT("averageFps"), Result.AverageFps);
	Frames->SetNumberField(TEXT("withinTarget"), Result.WithinTarget);
	Frames->SetNumberField(TEXT("hitches"), Result.Hitches);
	Frames->SetObjectField(TEXT("frame"), Timing(Result.Frame));
	Frames->SetObjectField(TEXT("gameThread"), Timing(Result.GameThread));
	Frames->SetObjectField(TEXT("renderThread"), Timing(Result.RenderThread));
	Frames->SetObjectField(TEXT("rhiThread"), Timing(Result.RhiThread));
	Frames->SetObjectField(TEXT("gpu"), Timing(Result.Gpu));
	Root->SetObjectField(TEXT("timing"), Frames);

	TSharedRef<FJsonObject> Audio = MakeShared<FJsonObject>();
	Audio->SetBoolField(TEXT("measured"), Result.bHasAudio);
	Audio->SetNumberField(TEXT("msPerBuffer"), Result.AudioMsPerBuffer);
	Audio->SetNumberField(TEXT("metaSoundMsPerBuffer"), Result.MetaSoundMsPerBuffer);
	Audio->SetNumberField(TEXT("load"), Result.AudioLoad);
	Root->SetObjectField(TEXT("audio"), Audio);

	TSharedRef<FJsonObject> Limits = MakeShared<FJsonObject>();
	Limits->SetNumberField(TEXT("targetFps"), Criteria->TargetFps);
	Limits->SetNumberField(TEXT("framePercentile"), Criteria->FramePercentile);
	Limits->SetNumberField(TEXT("targetFrameMs"), Criteria->TargetFrameMs());
	Limits->SetNumberField(TEXT("hitchMs"), Criteria->HitchMs);
	Limits->SetNumberField(TEXT("audioBufferBudgetMs"), Criteria->AudioBufferBudgetMs);
	Root->SetObjectField(TEXT("criteria"), Limits);

	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Root, Writer);
	const FString JsonPath = FPaths::ChangeExtension(CsvPath, TEXT("json"));
	UE_CLOG(!FFileHelper::SaveStringToFile(Json, *JsonPath), LogHollowGame, Error, TEXT("Hollow.Perf: cannot write %s"), *JsonPath);
	UE_LOG(LogHollowGame, Display, TEXT("Hollow.Perf: report %s"), *JsonPath);
}

bool UHollowPerfCaptureSubsystem::StartLookAround(const float Seconds, const float Turns)
{
	const APlayerController* Player = HollowPerfCapturePrivate::LocalPlayer(GetWorld());
	if (!Player || Seconds <= 0.f || LookTicker.IsValid())
	{
		return false;
	}
	LookStart = FPlatformTime::Seconds();
	LookSeconds = Seconds;
	LookTurns = Turns;
	LookFrom = Player->GetControlRotation();
	// The one per-frame step in the project, only while a sweep runs: a view that turns evenly with real time.
	LookTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float) { return StepLookAround(); }));
	return true;
}

bool UHollowPerfCaptureSubsystem::StepLookAround()
{
	APlayerController* Player = HollowPerfCapturePrivate::LocalPlayer(GetWorld());
	const double Elapsed = FPlatformTime::Seconds() - LookStart;
	if (!Player || Elapsed >= LookSeconds)
	{
		LookTicker.Reset();
		return false;
	}
	FRotator View = LookFrom;
	View.Yaw = FRotator::NormalizeAxis(LookFrom.Yaw + HollowPerfCapturePrivate::FullTurnDegrees * LookTurns * Elapsed / LookSeconds);
	Player->SetControlRotation(View);
	return true;
}

bool UHollowPerfCaptureSubsystem::StartAudioStress(const int32 Copies, const float Seconds)
{
	UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(this);
	const UHollowSoundBank* Bank = UHollowSoundBank::Get();
	const APlayerController* Player = HollowPerfCapturePrivate::LocalPlayer(GetWorld());
	const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	if (!Audio || !Bank || !Pawn || Copies <= 0 || Seconds <= 0.f || StressTimer.IsValid())
	{
		return false;
	}
	StressCopies = Copies;
	for (const FHollowSoundEntry& Entry : Bank->Entries)
	{
		const USoundBase* Sound = Entry.Sound.LoadSynchronous();
		for (int32 Copy = 0; Sound && !Sound->IsOneShot() && Copy < Copies; ++Copy)
		{
			if (UAudioComponent* Loop = Audio->PlayAttached(Entry.SoundTag, Pawn->GetRootComponent()))
			{
				StressLoops.Add(Loop);
			}
		}
	}
	FTimerManager& Timers = GetWorld()->GetTimerManager();
	Timers.SetTimer(StressTimer, this, &ThisClass::ReplayOneShots, GetDefault<UHollowPerfSettings>()->AudioStressRetriggerSeconds, /*bLoop=*/true, /*FirstDelay=*/0.f);
	Timers.SetTimer(StressEndTimer, this, &ThisClass::EndAudioStress, Seconds, /*bLoop=*/false);
	UE_LOG(LogHollowGame, Display, TEXT("Hollow.Audio.Stress: %d cop(ies) of every sound for %.0f s (%d loops)"), Copies, Seconds, StressLoops.Num());
	return true;
}

void UHollowPerfCaptureSubsystem::ReplayOneShots()
{
	UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(this);
	const UHollowSoundBank* Bank = UHollowSoundBank::Get();
	const APlayerController* Player = HollowPerfCapturePrivate::LocalPlayer(GetWorld());
	const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	if (!Audio || !Bank || !Pawn)
	{
		return;
	}
	for (const FHollowSoundEntry& Entry : Bank->Entries)
	{
		const USoundBase* Sound = Entry.Sound.Get();
		for (int32 Copy = 0; Sound && Sound->IsOneShot() && Copy < StressCopies; ++Copy)
		{
			Audio->Play(Entry.SoundTag, Pawn->GetActorLocation());
		}
	}
}

void UHollowPerfCaptureSubsystem::EndAudioStress()
{
	GetWorld()->GetTimerManager().ClearTimer(StressTimer);
	for (UAudioComponent* Loop : StressLoops)
	{
		if (Loop)
		{
			Loop->Stop();
		}
	}
	const FAudioDeviceHandle Device = GetWorld()->GetAudioDevice();
	UE_LOG(LogHollowGame, Display, TEXT("Hollow.Audio.Stress: done (%d loops stopped, %d source(s) still active)"), StressLoops.Num(), Device ? Device->GetNumActiveSources() : 0);
	StressLoops.Reset();
}
