#pragma once

#include "GameFramework/GameUserSettings.h"
#include "Settings/HollowGraphicsSettings.h"
#include "Voice/HollowVoiceRules.h"

#include "HollowGameUserSettings.generated.h"

struct FDisplayMetrics;

/**
 * The player's settings (GameUserSettings.ini) with the graphics preset (ADR-0023). The first launch on a machine picks
 * one (Steam Deck, else the hardware benchmark); -HollowPreset=Low|High overrides it for one run. Set as
 * GameUserSettingsClassName in DefaultEngine.ini.
 */
UCLASS(Config = GameUserSettings, ConfigDoNotCheckDefaults)
class HOLLOWGAME_API UHollowGameUserSettings : public UGameUserSettings
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
	bool IsReducedFlashes() const { return bReducedFlashes; }

	/** Switches it now (OnReducedFlashesChanged); bSave keeps it for the next launch. */
	void SetReducedFlashes(bool bReduced, bool bSave = true);

	/** The reduced-flashes choice changed (presentation, visor). */
	FSimpleMulticastDelegate OnReducedFlashesChanged;

	/** UX-01 (ADR-0042): whether the visor's controls cheat sheet is unfolded (H / View folds it). Unfolded until the player folds it. */
	bool IsControlsHintShown() const { return bControlsHintShown; }

	/** Folds or unfolds it now (OnControlsHintChanged); bSave keeps it for the next launch. */
	void SetControlsHintShown(bool bShown, bool bSave = true);

	FSimpleMulticastDelegate OnControlsHintChanged;

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
};
