#pragma once

#include "GameFramework/GameUserSettings.h"
#include "Interfaces/HollowComfortSettings.h"
#include "Settings/HollowGraphicsSettings.h"
#include "Voice/HollowVoiceRules.h"

#include "HollowGameUserSettings.generated.h"

struct FDisplayMetrics;
enum class EHollowLanguage : uint8;
enum class EHollowVolume : uint8;

/**
 * The player's settings (GameUserSettings.ini) with the graphics preset (ADR-0023). The first launch on a machine picks
 * one (Steam Deck, else the hardware benchmark); -HollowPreset=Low|High overrides it for one run. Set as
 * GameUserSettingsClassName in DefaultEngine.ini. The lower modules read the comfort choices through
 * IHollowComfortSettings (GP-01c: game feel in HollowCore). UI-02 (ADR-0052): the settings screen's controls, volumes,
 * frame rate limit, window mode and vertical sync; their ranges and choices are UHollowPlayerSettingsConfig's.
 */
UCLASS(Config = GameUserSettings, ConfigDoNotCheckDefaults)
class HOLLOWGAME_API UHollowGameUserSettings : public UGameUserSettings, public IHollowComfortSettings
{
	GENERATED_BODY()

public:
	/** The engine's settings object as ours, or null (another class configured). */
	static UHollowGameUserSettings* Get();

	/** Once after the engine starts, on machines that render: the saved preset, the first launch's choice, or the command line's. */
	void InitializePreset();

	/** Applies a preset now (scalability, resolution scale, dynamic resolution, frame limit); bSave keeps it for the next launch. */
	void ApplyPreset(EHollowGraphicsPreset Preset, bool bSave);

	EHollowGraphicsPreset GetPreset() const { return GraphicsPreset; }

	/** How the player's voice goes out (S4-07): push-to-talk unless the player chose otherwise. */
	EHollowVoiceMode GetVoiceMode() const { return VoiceMode; }

	/** Keeps the voice mode for the next launch. */
	void SetVoiceMode(EHollowVoiceMode Mode);

	/**
	 * Accessibility H-1 (ADR-0029): no fast flashes. The Resonance post-process, the emergency lights and the visor
	 * glow then change smoothly (at most 2 Hz) or hold steady, on this machine only. Off unless the player chose it.
	 */
	virtual bool IsReducedFlashes() const override { return bReducedFlashes; }

	/** Switches it now (OnReducedFlashesChanged); bSave keeps it for the next launch. */
	void SetReducedFlashes(bool bReduced, bool bSave = true);

	/** The reduced-flashes choice changed (presentation, visor). */
	FSimpleMulticastDelegate OnReducedFlashesChanged;

	/** UX-01 (ADR-0042): whether the visor's controls cheat sheet is unfolded (H / View folds it). Unfolded until the player folds it. */
	bool IsControlsHintShown() const { return bControlsHintShown; }

	/** Folds or unfolds it now (OnControlsHintChanged); bSave keeps it for the next launch. */
	void SetControlsHintShown(bool bShown, bool bSave = true);

	FSimpleMulticastDelegate OnControlsHintChanged;

	/**
	 * LOC-01 (ADR-0048): the language of the player's texts, applied once after the engine starts. The first launch takes
	 * the system's (Russian for any ru culture, English for any other) and keeps it; -culture=xx changes it for one run.
	 */
	void InitializeLanguage();

	/** The language the texts show in now. */
	EHollowLanguage GetLanguage() const;

	/** Shows the texts in Language now (HollowLanguage::OnApplied); bSave keeps it for the next launch. */
	void SetLanguage(EHollowLanguage Language, bool bSave = true);

	/** The kept language's culture name ("en", "ru"), empty before the first launch chose one. */
	const FString& GetSavedLanguage() const { return LanguageCulture; }

	/** -culture=xx, if given, as one of the game's languages. */
	static TOptional<EHollowLanguage> LanguageFromCommandLine();

	/**
	 * The language a start shows (InitializeLanguage): the command line's for this run, else the kept one, else the
	 * system's (Russian for any ru culture, English otherwise). bOutKeep: whether it is the first choice, to keep.
	 */
	static EHollowLanguage ChooseLanguage(const TOptional<EHollowLanguage>& CommandLine, const FString& Saved, const FString& SystemCulture, bool& bOutKeep);

	/**
	 * UI-02 (ADR-0052): mouse look speed, within UHollowPlayerSettingsConfig::MouseSensitivity (its default until the
	 * player chooses). UHollowLookSettingsModifier applies it to the mouse's look input.
	 */
	float GetMouseSensitivity() const;

	/** Keeps a sensitivity (snapped to the range) and tells OnControlsChanged; bSave keeps it for the next launch. */
	void SetMouseSensitivity(float Sensitivity, bool bSave = true);

	/** Looking up and down is inverted, mouse and gamepad alike. */
	bool IsLookInverted() const { return bInvertLook; }

	void SetLookInverted(bool bInverted, bool bSave = true);

	/** The player camera's horizontal field of view, degrees (UHollowPlayerSettingsConfig::FieldOfView). */
	float GetFieldOfView() const;

	void SetFieldOfView(float Degrees, bool bSave = true);

	/** The controls changed (sensitivity, inversion, field of view): the local ExoFrame applies its field of view. */
	FSimpleMulticastDelegate OnControlsChanged;

	/** A volume, linear (UHollowPlayerSettingsConfig::Volume); UHollowAudioVolumeSubsystem sets its bus. */
	float GetVolume(EHollowVolume Volume) const;

	void SetVolume(EHollowVolume Volume, float Value, bool bSave = true);

	/** A volume changed. */
	FSimpleMulticastDelegate OnAudioChanged;

	/** The frame rate limit in effect: the player's choice, else the preset's (the Steam Deck's on a Deck); 0: none. */
	float GetFrameRateChoice() const;

	/** Chooses a limit (one of UHollowPlayerSettingsConfig::FrameRateLimits): it outlives preset changes. Applied now. */
	void SetFrameRateChoice(float Limit, bool bSave = true);

	/**
	 * Fullscreen, windowed fullscreen or a window (UHollowPlayerSettingsConfig::WindowedResolutionScale of the monitor),
	 * applied now on a machine that renders.
	 */
	void SetWindowModeNow(EWindowMode::Type Mode, bool bSave = true);

	void SetVSyncNow(bool bEnabled, bool bSave = true);

	/** Tests: SaveSettings writes only this file's section instead of GameUserSettings.ini (empty: the real one). */
	void SetConfigFileForTests(const FString& Filename) { TestConfigFile = Filename; }

	virtual void SaveSettings() override;

	/** Only the preset's scalability and its console variables (what tests check: no window, no saving). */
	static bool ApplyScalability(const FHollowGraphicsPresetSpec& Spec);

	/** -HollowPreset=Low|High, if given. */
	static TOptional<EHollowGraphicsPreset> PresetFromCommandLine();

	/** Whether this is a Steam Deck (UHollowGraphicsSettings::DeckEnvironmentVariable is 1). */
	static bool IsSteamDeck();

	static FString PresetName(EHollowGraphicsPreset Preset);

	/**
	 * The monitor to remember when the settings name none (DisplayID empty, DisplayIndex -1): the primary one, or
	 * INDEX_NONE to keep the settings as they are. Without a known monitor the engine places a fullscreen window by
	 * FGenericWindow::GetFullScreenInfo, which on Mac (UE 5.8) answers a position below the screen: Slate then sends
	 * every click and hover elsewhere and no menu button can be pressed (S4-06 bug bash).
	 */
	static int32 DisplayToChoose(const FDisplayMetrics& Metrics, const FString& DisplayID, int32 DisplayIndex);

private:
	EHollowGraphicsPreset ChooseFirstPreset();

	/** Remembers the primary monitor if none is chosen and re-places the window on it; true if it changed the settings. */
	bool EnsureDisplayChosen();

	UPROPERTY(Config)
	EHollowGraphicsPreset GraphicsPreset = EHollowGraphicsPreset::High;

	UPROPERTY(Config)
	bool bGraphicsPresetChosen = false;

	UPROPERTY(Config)
	EHollowVoiceMode VoiceMode = EHollowVoiceMode::PushToTalk;

	UPROPERTY(Config)
	bool bReducedFlashes = false;

	UPROPERTY(Config)
	bool bControlsHintShown = true;

	/** The kept language ("en", "ru"); empty until the first launch chose one (LOC-01). */
	UPROPERTY(Config)
	FString LanguageCulture;

	/** UI-02: below 0, not chosen yet (the config's default). */
	UPROPERTY(Config)
	float MouseSensitivity = -1.f;

	UPROPERTY(Config)
	bool bInvertLook = false;

	UPROPERTY(Config)
	float FieldOfView = -1.f;

	UPROPERTY(Config)
	float MasterVolume = -1.f;

	UPROPERTY(Config)
	float EffectsVolume = -1.f;

	UPROPERTY(Config)
	float VoiceVolume = -1.f;

	/** Below 0: the preset's limit. */
	UPROPERTY(Config)
	float FrameRateLimitChoice = -1.f;

	FString TestConfigFile;
};
