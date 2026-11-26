#pragma once

#include "Engine/DeveloperSettings.h"

#include "HollowPlayerSettingsConfig.generated.h"

class USoundControlBus;

/** The three volumes of the settings screen (UI-02, ADR-0052); each is an Audio Modulation bus the sound classes listen to. */
UENUM(BlueprintType)
enum class EHollowVolume : uint8
{
	/** Every sound of the game. */
	Master,
	/** The world, the tools, the enemies and the interface (SC_Hollow_World, SC_Hollow_UI). */
	Effects,
	/** The other players' voices and the handler's radio (SC_Hollow_Voice). */
	Voice
};

/** A setting's range: the settings screen steps it by Step between Min and Max; Default before the player chose. */
USTRUCT(BlueprintType)
struct HOLLOWGAME_API FHollowSettingRange
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Range")
	float Min = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Range")
	float Max = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Range", meta = (ClampMin = "0.001"))
	float Step = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Range")
	float Default = 1.f;

	/** Value within the range, on its nearest step from Min. */
	float Snap(float Value) const;

	/** Value moved by Steps steps (negative: down), within the range. */
	float Stepped(float Value, int32 Steps) const;
};

/**
 * Ranges, choices and buses of the player's settings (UI-02, ADR-0052; DefaultGame.ini). UHollowGameUserSettings keeps
 * the player's values; what they start at and how far they go is here, not in code.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Hollow Player Settings"))
class HOLLOWGAME_API UHollowPlayerSettingsConfig : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Mouse look speed (a multiplier of the raw mouse movement; the gamepad's look speed is the input data's). */
	UPROPERTY(Config, EditAnywhere, Category = "Controls")
	FHollowSettingRange MouseSensitivity;

	/** Horizontal field of view of the player's camera, degrees (the first-person hands keep their own). */
	UPROPERTY(Config, EditAnywhere, Category = "Controls")
	FHollowSettingRange FieldOfView;

	/** Every volume, linear (1: as authored). */
	UPROPERTY(Config, EditAnywhere, Category = "Audio")
	FHollowSettingRange Volume;

	/** The frame rate limits the player can choose, in order; 0: none. */
	UPROPERTY(Config, EditAnywhere, Category = "Graphics")
	TArray<float> FrameRateLimits;

	/** A window (not fullscreen) is this share of the monitor's resolution. */
	UPROPERTY(Config, EditAnywhere, Category = "Graphics", meta = (ClampMin = "0.25", ClampMax = "1"))
	float WindowedResolutionScale = 1.f;

	UPROPERTY(Config, EditAnywhere, Category = "Audio")
	TSoftObjectPtr<USoundControlBus> MasterVolumeBus;

	UPROPERTY(Config, EditAnywhere, Category = "Audio")
	TSoftObjectPtr<USoundControlBus> EffectsVolumeBus;

	UPROPERTY(Config, EditAnywhere, Category = "Audio")
	TSoftObjectPtr<USoundControlBus> VoiceVolumeBus;

	/** A volume change fades this long (the first one of a map is at once). */
	UPROPERTY(Config, EditAnywhere, Category = "Audio", meta = (ClampMin = "0", Units = "Seconds"))
	float VolumeFadeSeconds = 0.f;

	const TSoftObjectPtr<USoundControlBus>& GetBus(EHollowVolume Which) const;
};
