#pragma once

#include "GameFramework/GameUserSettings.h"
#include "Settings/HollowGraphicsSettings.h"

#include "HollowGameUserSettings.generated.h"

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

	/** Only the preset's scalability and its console variables (what tests check: no window, no saving). */
	static bool ApplyScalability(const FHollowGraphicsPresetSpec& Spec);

	/** -HollowPreset=Low|High, if given. */
	static TOptional<EHollowGraphicsPreset> PresetFromCommandLine();

	/** Whether this is a Steam Deck (UHollowGraphicsSettings::DeckEnvironmentVariable is 1). */
	static bool IsSteamDeck();

	static FString PresetName(EHollowGraphicsPreset Preset);

private:
	EHollowGraphicsPreset ChooseFirstPreset();

	UPROPERTY(Config)
	EHollowGraphicsPreset GraphicsPreset = EHollowGraphicsPreset::High;

	UPROPERTY(Config)
	bool bGraphicsPresetChosen = false;
};
