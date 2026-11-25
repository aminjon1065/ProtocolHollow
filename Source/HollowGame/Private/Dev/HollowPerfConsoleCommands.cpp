#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "HollowGameLog.h"
#include "Perf/HollowFrameStats.h"
#include "Perf/HollowPerfCaptureSubsystem.h"
#include "RHI.h"
#include "RHIStats.h"
#include "Scalability.h"
#include "Settings/HollowGameUserSettings.h"
#include "Settings/HollowGraphicsSettings.h"
#include "UnrealClient.h"

#if !UE_BUILD_SHIPPING

// Performance tools (ADR-0023): captures, graphics presets, the camera sweep and the audio stress of the benchmark scenario.
namespace HollowPerfConsolePrivate
{
	constexpr double BytesPerMegabyte = 1024.0 * 1024.0;

	/** Usage: Hollow.Perf.Capture <Name> [Seconds] [WarmupSeconds] */
	void Capture(const TArray<FString>& Args, UWorld* World)
	{
		UHollowPerfCaptureSubsystem* Perf = UHollowPerfCaptureSubsystem::Get(World);
		const UHollowPerfSettings* Settings = GetDefault<UHollowPerfSettings>();
		if (!Perf || Args.IsEmpty())
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Usage (in a game world): Hollow.Perf.Capture <Name> [Seconds] [WarmupSeconds]"));
			return;
		}
		const float Seconds = Args.IsValidIndex(1) ? FCString::Atof(*Args[1]) : Settings->CaptureSeconds;
		const float Warmup = Args.IsValidIndex(2) ? FCString::Atof(*Args[2]) : Settings->WarmupSeconds;
		Perf->StartCapture(Args[0], Seconds, Warmup);
	}

	/** Usage: Hollow.Graphics.Preset <Low|High> [Save] */
	void Preset(const TArray<FString>& Args, UWorld* World)
	{
		UHollowGameUserSettings* Settings = UHollowGameUserSettings::Get();
		const int64 Value = Args.IsEmpty() ? INDEX_NONE : StaticEnum<EHollowGraphicsPreset>()->GetValueByNameString(Args[0]);
		if (!Settings || Value == INDEX_NONE)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Usage: Hollow.Graphics.Preset <Low|High> [Save] (needs UHollowGameUserSettings)"));
			return;
		}
		Settings->ApplyPreset(static_cast<EHollowGraphicsPreset>(Value), Args.IsValidIndex(1) && Args[1].Equals(TEXT("Save"), ESearchCase::IgnoreCase));
	}

	/** The preset, the scalability levels, the console variables the presets differ in, the GPU and the view. */
	void Dump(UWorld* World)
	{
		const UHollowGameUserSettings* Settings = UHollowGameUserSettings::Get();
		const Scalability::FQualityLevels Levels = Scalability::GetQualityLevels();
		const TOptional<EHollowGraphicsPreset> Forced = UHollowGameUserSettings::PresetFromCommandLine();
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Graphics.Dump: preset %s%s%s; scalability view %d AA %d shadow %d GI %d reflection %d post %d texture %d effects %d shading %d; resolution %.0f %%"),
			Settings ? *UHollowGameUserSettings::PresetName(Settings->GetPreset()) : TEXT("unknown"), Forced ? TEXT(" (command line)") : TEXT(""),
			UHollowGameUserSettings::IsSteamDeck() ? TEXT(" (Steam Deck)") : TEXT(""), Levels.ViewDistanceQuality, Levels.AntiAliasingQuality, Levels.ShadowQuality,
			Levels.GlobalIlluminationQuality, Levels.ReflectionQuality, Levels.PostProcessQuality, Levels.TextureQuality, Levels.EffectsQuality, Levels.ShadingQuality,
			Levels.ResolutionQuality);
		FString Variables;
		for (const FString& Name : GetDefault<UHollowGraphicsSettings>()->DumpConsoleVariables)
		{
			const IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(*Name);
			Variables += FString::Printf(TEXT(" %s=%s"), *Name, Variable ? *Variable->GetString() : TEXT("?"));
		}
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Graphics.Dump:%s"), *Variables);
		FTextureMemoryStats Memory;
		RHIGetTextureMemoryStats(Memory);
		const FViewport* Viewport = GEngine && GEngine->GameViewport ? GEngine->GameViewport->Viewport : nullptr;
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Graphics.Dump: %s on %s, %.0f MB VRAM, view %s"), GDynamicRHI ? GDynamicRHI->GetName() : TEXT("no RHI"), *GRHIAdapterName,
			static_cast<double>(Memory.DedicatedVideoMemory) / BytesPerMegabyte, Viewport ? *Viewport->GetSizeXY().ToString() : TEXT("none"));
	}

	/** Usage: Hollow.Dev.LookAround <Seconds> [Turns] */
	void LookAround(const TArray<FString>& Args, UWorld* World)
	{
		UHollowPerfCaptureSubsystem* Perf = UHollowPerfCaptureSubsystem::Get(World);
		const float Seconds = Args.IsValidIndex(0) ? FCString::Atof(*Args[0]) : 0.f;
		const float Turns = Args.IsValidIndex(1) ? FCString::Atof(*Args[1]) : 1.f;
		if (!Perf || !Perf->StartLookAround(Seconds, Turns))
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Usage (with a local player, one sweep at a time): Hollow.Dev.LookAround <Seconds> [Turns]"));
			return;
		}
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Dev.LookAround: %.1f turn(s) in %.0f s"), Turns, Seconds);
	}

	/** Usage: Hollow.Dev.HideTagged <Tag> [1|0]: this machine hides (or shows again) the actors tagged Tag, for an A/B capture. */
	void HideTagged(const TArray<FString>& Args, UWorld* World)
	{
		if (!World || Args.IsEmpty())
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Usage: Hollow.Dev.HideTagged <Tag> [1|0]"));
			return;
		}
		const FName Tag(*Args[0]);
		const bool bHide = !Args.IsValidIndex(1) || Args[1].ToBool();
		int32 Count = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->Tags.Contains(Tag))
			{
				It->SetActorHiddenInGame(bHide);
				++Count;
			}
		}
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Dev.HideTagged: %d actor(s) tagged %s %s"), Count, *Tag.ToString(), bHide ? TEXT("hidden") : TEXT("shown"));
	}

	/** Usage: Hollow.Audio.Stress <Copies> <Seconds> */
	void AudioStress(const TArray<FString>& Args, UWorld* World)
	{
		UHollowPerfCaptureSubsystem* Perf = UHollowPerfCaptureSubsystem::Get(World);
		const int32 Copies = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 0;
		const float Seconds = Args.IsValidIndex(1) ? FCString::Atof(*Args[1]) : 0.f;
		if (!Perf || !Perf->StartAudioStress(Copies, Seconds))
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Usage (with a local pawn and an audio subsystem, one at a time): Hollow.Audio.Stress <Copies> <Seconds>"));
		}
	}

	FAutoConsoleCommandWithWorldAndArgs CaptureCommand(
		TEXT("Hollow.Perf.Capture"),
		TEXT("Records the CSV profiler for Seconds after WarmupSeconds (UHollowPerfSettings defaults) and judges it against E9: a PASS/FAIL line and a JSON report in Saved/Profiling/Hollow. Usage: Hollow.Perf.Capture <Name> [Seconds] [WarmupSeconds]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Capture));

	FAutoConsoleCommandWithWorldAndArgs PresetCommand(
		TEXT("Hollow.Graphics.Preset"),
		TEXT("Applies a graphics preset now (Save keeps it in GameUserSettings.ini). Usage: Hollow.Graphics.Preset <Low|High> [Save]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Preset));

	FAutoConsoleCommandWithWorld DumpCommand(
		TEXT("Hollow.Graphics.Dump"),
		TEXT("Prints the graphics preset, the scalability levels, the console variables the presets set, the RHI, GPU, VRAM and the view size."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Dump));

	FAutoConsoleCommandWithWorldAndArgs LookAroundCommand(
		TEXT("Hollow.Dev.LookAround"),
		TEXT("Turns the local player's view evenly round Turns times over Seconds (repeatable views for captures). Usage: Hollow.Dev.LookAround <Seconds> [Turns]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&LookAround));

	FAutoConsoleCommandWithWorldAndArgs HideTaggedCommand(
		TEXT("Hollow.Dev.HideTagged"),
		TEXT("This machine hides (1, the default) or shows again (0) every actor tagged Tag in the loaded levels: an A/B capture of a dressing (FAB-01: HollowFab). Usage: Hollow.Dev.HideTagged <Tag> [1|0]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HideTagged));

	FAutoConsoleCommandWithWorldAndArgs AudioStressCommand(
		TEXT("Hollow.Audio.Stress"),
		TEXT("Plays every bank sound Copies times at the local player for Seconds (loops held, one-shots replayed): the MetaSound CPU cost under load. Usage: Hollow.Audio.Stress <Copies> <Seconds>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AudioStress));
}

#endif // !UE_BUILD_SHIPPING
