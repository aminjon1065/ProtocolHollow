#include "Menus/HollowSettingsRules.h"

#include "GameFramework/PlayerController.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Localization/HollowLanguage.h"
#include "Menus/HollowSettingsLayout.h"
#include "Settings/HollowGameUserSettings.h"
#include "Settings/HollowGraphicsSettings.h"
#include "Settings/HollowPlayerSettingsConfig.h"
#include "Voice/HollowVoiceComponent.h"
#include "Voice/HollowVoiceRules.h"

namespace HollowSettingsRulesPrivate
{
	constexpr EWindowMode::Type WindowModes[] = {EWindowMode::Fullscreen, EWindowMode::WindowedFullscreen, EWindowMode::Windowed};
	constexpr EHollowVoiceMode VoiceModes[] = {EHollowVoiceMode::PushToTalk, EHollowVoiceMode::OpenMic, EHollowVoiceMode::Muted};
	constexpr EHollowGraphicsPreset Presets[] = {EHollowGraphicsPreset::High, EHollowGraphicsPreset::Low};

	/** The index Direction steps from Index among Count choices, wrapping round. */
	int32 Wrap(const int32 Index, const int32 Direction, const int32 Count)
	{
		return Count > 0 ? ((Index + (Direction > 0 ? 1 : -1)) % Count + Count) % Count : 0;
	}

	template <typename T, int32 N>
	constexpr int32 Count(const T (&)[N])
	{
		return N;
	}

	template <typename T, int32 N>
	int32 IndexOf(const T (&Choices)[N], const T Value)
	{
		for (int32 Index = 0; Index < N; ++Index)
		{
			if (Choices[Index] == Value)
			{
				return Index;
			}
		}
		return 0;
	}

	/** The listed frame rate limit nearest to Limit (the preset's limit may be none of them). */
	int32 NearestLimit(const TArray<float>& Limits, const float Limit)
	{
		int32 Best = 0;
		for (int32 Index = 1; Index < Limits.Num(); ++Index)
		{
			if (FMath::Abs(Limits[Index] - Limit) < FMath::Abs(Limits[Best] - Limit))
			{
				Best = Index;
			}
		}
		return Best;
	}

	EHollowVolume VolumeOf(const EHollowSettingId Setting)
	{
		return Setting == EHollowSettingId::EffectsVolume ? EHollowVolume::Effects : Setting == EHollowSettingId::VoiceVolume ? EHollowVolume::Voice : EHollowVolume::Master;
	}

	FText OnOff(const bool bOn, const FHollowSettingsTexts& Texts)
	{
		return bOn ? Texts.On : Texts.Off;
	}
}

bool HollowSettingsRules::IsChoice(const EHollowSettingId Setting)
{
	switch (Setting)
	{
	case EHollowSettingId::MasterVolume:
	case EHollowSettingId::EffectsVolume:
	case EHollowSettingId::VoiceVolume:
	case EHollowSettingId::MouseSensitivity:
	case EHollowSettingId::FieldOfView:
		return false;
	default:
		return true;
	}
}

FText HollowSettingsRules::LanguageName(const EHollowLanguage Language)
{
	const FCulturePtr Culture = FInternationalization::Get().GetCulture(HollowLanguage::CultureName(Language));
	const FString Native = Culture.IsValid() ? Culture->GetNativeLanguage() : FString(HollowLanguage::CultureName(Language));
	if (Native.IsEmpty())
	{
		return FText::GetEmpty();
	}
	// ICU names languages in lower case ("russkiy"): a capital first letter, by ICU too (Cyrillic included).
	const FString First = FText::AsCultureInvariant(Native.Left(1)).ToUpper().ToString();
	return FText::AsCultureInvariant(First + Native.RightChop(1));
}

FText HollowSettingsRules::ValueText(const EHollowSettingId Setting, const UHollowGameUserSettings& Settings, const FHollowSettingsTexts& Texts)
{
	using namespace HollowSettingsRulesPrivate;
	switch (Setting)
	{
	case EHollowSettingId::GraphicsPreset:
		return Settings.GetPreset() == EHollowGraphicsPreset::Low ? Texts.PresetLow : Texts.PresetHigh;
	case EHollowSettingId::WindowMode:
		switch (Settings.GetFullscreenMode())
		{
		case EWindowMode::Fullscreen:
			return Texts.Fullscreen;
		case EWindowMode::Windowed:
			return Texts.Windowed;
		default:
			return Texts.WindowedFullscreen;
		}
	case EHollowSettingId::VSync:
		return OnOff(Settings.IsVSyncEnabled(), Texts);
	case EHollowSettingId::FrameRateLimit:
	{
		const float Limit = Settings.GetFrameRateChoice();
		return Limit > 0.f ? FText::Format(Texts.FramesFormat, FText::AsNumber(FMath::RoundToInt32(Limit))) : Texts.Unlimited;
	}
	case EHollowSettingId::MasterVolume:
	case EHollowSettingId::EffectsVolume:
	case EHollowSettingId::VoiceVolume:
	{
		FNumberFormattingOptions Whole;
		Whole.MaximumFractionalDigits = 0;
		return FText::AsPercent(Settings.GetVolume(VolumeOf(Setting)), &Whole);
	}
	case EHollowSettingId::VoiceMode:
		switch (Settings.GetVoiceMode())
		{
		case EHollowVoiceMode::OpenMic:
			return Texts.OpenMic;
		case EHollowVoiceMode::Muted:
			return Texts.Muted;
		default:
			return Texts.PushToTalk;
		}
	case EHollowSettingId::MouseSensitivity:
	{
		FNumberFormattingOptions Tenths;
		Tenths.MinimumFractionalDigits = 1;
		Tenths.MaximumFractionalDigits = 1;
		return FText::AsNumber(Settings.GetMouseSensitivity(), &Tenths);
	}
	case EHollowSettingId::InvertLook:
		return OnOff(Settings.IsLookInverted(), Texts);
	case EHollowSettingId::FieldOfView:
		return FText::Format(Texts.DegreesFormat, FText::AsNumber(FMath::RoundToInt32(Settings.GetFieldOfView())));
	case EHollowSettingId::ControlsHint:
		return OnOff(Settings.IsControlsHintShown(), Texts);
	case EHollowSettingId::Language:
		return LanguageName(Settings.GetLanguage());
	case EHollowSettingId::ReducedFlashes:
		return OnOff(Settings.IsReducedFlashes(), Texts);
	default:
		return FText::GetEmpty();
	}
}

bool HollowSettingsRules::Step(const EHollowSettingId Setting, UHollowGameUserSettings& Settings, const int32 Direction, APlayerController* Player)
{
	using namespace HollowSettingsRulesPrivate;
	if (Direction == 0)
	{
		return false;
	}
	const UHollowPlayerSettingsConfig* Config = GetDefault<UHollowPlayerSettingsConfig>();
	const int32 Steps = Direction > 0 ? 1 : -1;
	switch (Setting)
	{
	case EHollowSettingId::GraphicsPreset:
		Settings.ApplyPreset(Presets[Wrap(IndexOf(Presets, Settings.GetPreset()), Steps, Count(Presets))], /*bSave=*/true);
		return true;
	case EHollowSettingId::WindowMode:
		Settings.SetWindowModeNow(WindowModes[Wrap(IndexOf(WindowModes, Settings.GetFullscreenMode()), Steps, Count(WindowModes))]);
		return true;
	case EHollowSettingId::VSync:
		Settings.SetVSyncNow(!Settings.IsVSyncEnabled());
		return true;
	case EHollowSettingId::FrameRateLimit:
	{
		const TArray<float>& Limits = Config->FrameRateLimits;
		if (Limits.IsEmpty())
		{
			return false;
		}
		Settings.SetFrameRateChoice(Limits[Wrap(NearestLimit(Limits, Settings.GetFrameRateChoice()), Steps, Limits.Num())]);
		return true;
	}
	case EHollowSettingId::MasterVolume:
	case EHollowSettingId::EffectsVolume:
	case EHollowSettingId::VoiceVolume:
	{
		const EHollowVolume Volume = VolumeOf(Setting);
		const float Before = Settings.GetVolume(Volume);
		const float After = Config->Volume.Stepped(Before, Steps);
		if (FMath::IsNearlyEqual(Before, After))
		{
			return false;
		}
		Settings.SetVolume(Volume, After);
		return true;
	}
	case EHollowSettingId::VoiceMode:
	{
		const EHollowVoiceMode Mode = VoiceModes[Wrap(IndexOf(VoiceModes, Settings.GetVoiceMode()), Steps, Count(VoiceModes))];
		if (UHollowVoiceComponent* Voice = UHollowVoiceComponent::Get(Player))
		{
			Voice->SetMode(Mode);   // the live voice now; it keeps the mode in the player's settings
		}
		Settings.SetVoiceMode(Mode);
		return true;
	}
	case EHollowSettingId::MouseSensitivity:
	{
		const float Before = Settings.GetMouseSensitivity();
		const float After = Config->MouseSensitivity.Stepped(Before, Steps);
		if (FMath::IsNearlyEqual(Before, After))
		{
			return false;
		}
		Settings.SetMouseSensitivity(After);
		return true;
	}
	case EHollowSettingId::InvertLook:
		Settings.SetLookInverted(!Settings.IsLookInverted());
		return true;
	case EHollowSettingId::FieldOfView:
	{
		const float Before = Settings.GetFieldOfView();
		const float After = Config->FieldOfView.Stepped(Before, Steps);
		if (FMath::IsNearlyEqual(Before, After))
		{
			return false;
		}
		Settings.SetFieldOfView(After);
		return true;
	}
	case EHollowSettingId::ControlsHint:
		Settings.SetControlsHintShown(!Settings.IsControlsHintShown());
		return true;
	case EHollowSettingId::Language:
	{
		const TConstArrayView<EHollowLanguage> All = HollowLanguage::All();
		const int32 Index = All.IndexOfByKey(Settings.GetLanguage());
		Settings.SetLanguage(All[Wrap(Index == INDEX_NONE ? 0 : Index, Steps, All.Num())]);
		return true;
	}
	case EHollowSettingId::ReducedFlashes:
		Settings.SetReducedFlashes(!Settings.IsReducedFlashes());
		return true;
	default:
		return false;
	}
}
