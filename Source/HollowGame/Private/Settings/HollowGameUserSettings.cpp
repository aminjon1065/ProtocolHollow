#include "Settings/HollowGameUserSettings.h"

#include "Engine/Engine.h"
#include "GenericPlatform/GenericApplication.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HollowGameLog.h"
#include "Localization/HollowLanguage.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Scalability.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowGameUserSettings)

namespace HollowGameUserSettingsPrivate
{
	const TCHAR* const PresetSwitch = TEXT("HollowPreset=");
	const TCHAR* const DeckOn = TEXT("1");

	void SetCVar(const TCHAR* Name, const float Value)
	{
		if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Variable->Set(Value, ECVF_SetByGameSetting);
		}
	}

	/** Dynamic resolution's range and frame budget (outside the scalability groups). */
	void ApplyResolutionCVars(const FHollowGraphicsPresetSpec& Spec, const float BudgetMs)
	{
		SetCVar(TEXT("r.DynamicRes.MaxScreenPercentage"), Spec.ScreenPercentage);
		SetCVar(TEXT("r.DynamicRes.MinScreenPercentage"), Spec.bDynamicResolution ? Spec.MinScreenPercentage : Spec.ScreenPercentage);
		if (Spec.bDynamicResolution && BudgetMs > 0.f)
		{
			SetCVar(TEXT("r.DynamicRes.FrameTimeBudget"), BudgetMs);
		}
	}
}

UHollowGameUserSettings* UHollowGameUserSettings::Get()
{
	return GEngine ? Cast<UHollowGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}

FString UHollowGameUserSettings::PresetName(const EHollowGraphicsPreset Preset)
{
	return StaticEnum<EHollowGraphicsPreset>()->GetNameStringByValue(static_cast<int64>(Preset));
}

TOptional<EHollowGraphicsPreset> UHollowGameUserSettings::PresetFromCommandLine()
{
	FString Value;
	if (!FParse::Value(FCommandLine::Get(), HollowGameUserSettingsPrivate::PresetSwitch, Value))
	{
		return {};
	}
	const int64 Index = StaticEnum<EHollowGraphicsPreset>()->GetValueByNameString(Value);   // any case
	if (Index == INDEX_NONE)
	{
		UE_LOG(LogHollowGame, Warning, TEXT("-%s%s: not a preset (Low, High)."), HollowGameUserSettingsPrivate::PresetSwitch, *Value);
		return {};
	}
	return static_cast<EHollowGraphicsPreset>(Index);
}

bool UHollowGameUserSettings::IsSteamDeck()
{
	const FString& Variable = GetDefault<UHollowGraphicsSettings>()->DeckEnvironmentVariable;
	return !Variable.IsEmpty() && FPlatformMisc::GetEnvironmentVariable(*Variable) == HollowGameUserSettingsPrivate::DeckOn;
}

bool UHollowGameUserSettings::ApplyScalability(const FHollowGraphicsPresetSpec& Spec)
{
	Scalability::FQualityLevels Levels = Scalability::GetQualityLevels();
	Levels.SetFromSingleQualityLevel(Spec.ScalabilityLevel);
	Levels.ResolutionQuality = Spec.ScreenPercentage;
	Scalability::SetQualityLevels(Levels, /*bForce=*/true);
	HollowGameUserSettingsPrivate::ApplyResolutionCVars(Spec, Spec.DynamicResolutionBudgetMs);
	IConsoleManager::Get().CallAllConsoleVariableSinks();
	// Every group at the level (the single level reads "custom" once the resolution differs from the level's default).
	Scalability::FQualityLevels Applied = Scalability::GetQualityLevels();
	Applied.ResolutionQuality = Levels.ResolutionQuality;
	return Applied == Levels;
}

void UHollowGameUserSettings::ApplyPreset(const EHollowGraphicsPreset Preset, const bool bSave)
{
	const UHollowGraphicsSettings* Graphics = GetDefault<UHollowGraphicsSettings>();
	const FHollowGraphicsPresetSpec* Spec = Graphics->FindPreset(Preset);
	if (!Spec)
	{
		UE_LOG(LogHollowGame, Warning, TEXT("Graphics preset %s is not in UHollowGraphicsSettings::Presets."), *PresetName(Preset));
		return;
	}
	GraphicsPreset = Preset;
	const bool bDeck = IsSteamDeck();
	const float FrameLimit = bDeck ? Graphics->DeckFrameRateLimit : Spec->FrameRateLimit;
	SetOverallScalabilityLevel(Spec->ScalabilityLevel);
	SetResolutionScaleValueEx(Spec->ScreenPercentage);
	SetDynamicResolutionEnabled(Spec->bDynamicResolution);
	SetFrameRateLimit(FrameLimit);
	if (bDeck && Graphics->DeckResolution.GetMin() > 0)
	{
		SetScreenResolution(Graphics->DeckResolution);
		SetFullscreenMode(EWindowMode::Fullscreen);
		ApplyResolutionSettings(/*bCheckForCommandLineOverrides=*/true);
	}
	ApplyNonResolutionSettings();
	// On the Deck the dynamic resolution aims at its own frame rate.
	HollowGameUserSettingsPrivate::ApplyResolutionCVars(*Spec, bDeck && FrameLimit > 0.f ? 1000.f / FrameLimit : Spec->DynamicResolutionBudgetMs);
	if (bSave)
	{
		SaveSettings();
	}
	UE_LOG(LogHollowGame, Display, TEXT("Graphics preset %s: scalability %d, %.0f %% resolution, dynamic resolution %s, frame limit %s%s"), *PresetName(Preset),
		Spec->ScalabilityLevel, Spec->ScreenPercentage, Spec->bDynamicResolution ? TEXT("on") : TEXT("off"),
		FrameLimit > 0.f ? *FString::Printf(TEXT("%.0f"), FrameLimit) : TEXT("none"), bDeck ? TEXT(" (Steam Deck)") : TEXT(""));
}

EHollowGraphicsPreset UHollowGameUserSettings::ChooseFirstPreset()
{
	if (IsSteamDeck())
	{
		UE_LOG(LogHollowGame, Display, TEXT("First launch on a Steam Deck: Low."));
		return EHollowGraphicsPreset::Low;
	}
	RunHardwareBenchmark();
	const float Score = GetLastGPUBenchmarkResult();
	const EHollowGraphicsPreset Preset = GetDefault<UHollowGraphicsSettings>()->PresetForBenchmark(Score);
	UE_LOG(LogHollowGame, Display, TEXT("First launch: GPU benchmark %.0f -> %s."), Score, *PresetName(Preset));
	return Preset;
}

int32 UHollowGameUserSettings::DisplayToChoose(const FDisplayMetrics& Metrics, const FString& DisplayID, const int32 DisplayIndex)
{
	if (!DisplayID.IsEmpty() || DisplayIndex != INDEX_NONE)
	{
		return INDEX_NONE;   // the player's (or an earlier launch's) choice stays
	}
	return Metrics.MonitorInfo.IndexOfByPredicate([](const FMonitorInfo& Monitor) { return Monitor.bIsPrimary; });
}

bool UHollowGameUserSettings::EnsureDisplayChosen()
{
	FDisplayMetrics Metrics;
	FDisplayMetrics::RebuildDisplayMetrics(Metrics);
	const int32 Monitor = DisplayToChoose(Metrics, GetDisplayID(), GetDisplayIndex());
	if (Monitor == INDEX_NONE)
	{
		return false;
	}
	SetDisplayProperties(Metrics.MonitorInfo[Monitor].ID, Monitor);
	ApplyResolutionSettings(/*bCheckForCommandLineOverrides=*/true);   // the window already exists: place it by the monitor now
	UE_LOG(LogHollowGame, Display, TEXT("Display: %s (monitor %d) remembered for the game window."), *Metrics.MonitorInfo[Monitor].Name, Monitor);
	return true;
}

void UHollowGameUserSettings::SetVoiceMode(const EHollowVoiceMode Mode)
{
	if (Mode != VoiceMode)
	{
		VoiceMode = Mode;
		SaveSettings();
	}
}

void UHollowGameUserSettings::SetReducedFlashes(const bool bReduced, const bool bSave)
{
	if (bReduced == bReducedFlashes)
	{
		return;
	}
	bReducedFlashes = bReduced;
	if (bSave)
	{
		SaveSettings();
	}
	UE_LOG(LogHollowGame, Display, TEXT("Reduced flashes %s"), bReduced ? TEXT("on") : TEXT("off"));
	OnReducedFlashesChanged.Broadcast();
}

void UHollowGameUserSettings::SetControlsHintShown(const bool bShown, const bool bSave)
{
	if (bShown == bControlsHintShown)
	{
		return;
	}
	bControlsHintShown = bShown;
	if (bSave)
	{
		SaveSettings();
	}
	UE_LOG(LogHollowGame, Display, TEXT("Controls cheat sheet %s"), bShown ? TEXT("shown") : TEXT("folded"));
	OnControlsHintChanged.Broadcast();
}

void UHollowGameUserSettings::InitializeLanguage()
{
	bool bKeep = false;
	const EHollowLanguage Language = ChooseLanguage(LanguageFromCommandLine(), LanguageCulture, FPlatformMisc::GetDefaultLanguage(), bKeep);
	SetLanguage(Language, bKeep);
}

EHollowLanguage UHollowGameUserSettings::ChooseLanguage(const TOptional<EHollowLanguage>& CommandLine, const FString& Saved,
	const FString& SystemCulture, bool& bOutKeep)
{
	bOutKeep = false;
	if (CommandLine.IsSet())
	{
		return CommandLine.GetValue();   // this run only (screenshots, tests)
	}
	if (const TOptional<EHollowLanguage> Kept = HollowLanguage::Parse(Saved))
	{
		return Kept.GetValue();
	}
	bOutKeep = true;   // the first launch: the system's, kept
	return HollowLanguage::FromCulture(SystemCulture);
}

EHollowLanguage UHollowGameUserSettings::GetLanguage() const
{
	return HollowLanguage::GetApplied();
}

void UHollowGameUserSettings::SetLanguage(const EHollowLanguage Language, const bool bSave)
{
	if (bSave)
	{
		LanguageCulture = HollowLanguage::CultureName(Language);
		SaveSettings();
	}
	HollowLanguage::Apply(Language);
}

TOptional<EHollowLanguage> UHollowGameUserSettings::LanguageFromCommandLine()
{
	FString Culture;
	if (!FParse::Value(FCommandLine::Get(), TEXT("-culture="), Culture) || Culture.IsEmpty())
	{
		return {};
	}
	return HollowLanguage::FromCulture(Culture);
}

void UHollowGameUserSettings::InitializePreset()
{
	if (!FApp::CanEverRender())
	{
		return;   // a headless client or server: nothing to show, nothing to save
	}
	if (EnsureDisplayChosen())
	{
		SaveSettings();   // the next launch opens its window on that monitor from the start
	}
	if (const TOptional<EHollowGraphicsPreset> Forced = PresetFromCommandLine())
	{
		ApplyPreset(Forced.GetValue(), /*bSave=*/false);   // this run only
		return;
	}
	if (!bGraphicsPresetChosen)
	{
		GraphicsPreset = ChooseFirstPreset();
		bGraphicsPresetChosen = true;
		ApplyPreset(GraphicsPreset, /*bSave=*/true);
		return;
	}
	ApplyPreset(GraphicsPreset, /*bSave=*/false);
}
