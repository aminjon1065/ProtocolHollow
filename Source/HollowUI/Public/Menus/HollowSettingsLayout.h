#pragma once

#include "Engine/DataAsset.h"

#include "HollowSettingsLayout.generated.h"

/** One setting of the settings screen (UI-02, ADR-0052); HollowSettingsRules reads and steps it. */
UENUM(BlueprintType)
enum class EHollowSettingId : uint8
{
	GraphicsPreset,
	WindowMode,
	VSync,
	FrameRateLimit,
	MasterVolume,
	EffectsVolume,
	VoiceVolume,
	VoiceMode,
	MouseSensitivity,
	InvertLook,
	FieldOfView,
	ControlsHint,
	Language,
	ReducedFlashes,
	/** POLISH-01 (ADR-0053): full or immersive waypoints. */
	Waypoints
};

/** A row of a tab: its setting and its name (a string table entry). */
USTRUCT(BlueprintType)
struct HOLLOWUI_API FHollowSettingsRowSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	EHollowSettingId Setting = EHollowSettingId::GraphicsPreset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	FText Label;
};

USTRUCT(BlueprintType)
struct HOLLOWUI_API FHollowSettingsTabSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	TArray<FHollowSettingsRowSpec> Rows;
};

/** The values the rows show in words (string table entries); numbers and percentages are formatted for the language. */
USTRUCT(BlueprintType)
struct HOLLOWUI_API FHollowSettingsTexts
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Texts")
	FText On;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Texts")
	FText Off;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Texts")
	FText PresetLow;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Texts")
	FText PresetHigh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Texts")
	FText Fullscreen;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Texts")
	FText WindowedFullscreen;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Texts")
	FText Windowed;

	/** No frame rate limit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Texts")
	FText Unlimited;

	/** "{0} FPS". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Texts")
	FText FramesFormat;

	/** "{0} degrees" with the degree sign. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Texts")
	FText DegreesFormat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Texts")
	FText PushToTalk;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Texts")
	FText OpenMic;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Texts")
	FText Muted;

	/** POLISH-01: the waypoints' modes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Texts")
	FText WaypointsFull;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Texts")
	FText WaypointsImmersive;
};

/**
 * The settings screen's tabs and rows (DA_SettingsLayout, create_ui_assets.py): which setting shows on which tab, in
 * what order, under which name. Adding a row to a tab needs no code; a new kind of setting does (HollowSettingsRules).
 */
UCLASS(BlueprintType)
class HOLLOWUI_API UHollowSettingsLayout : public UDataAsset
{
	GENERATED_BODY()

public:
	const FHollowSettingsTabSpec* FindTab(FName Id) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings", meta = (TitleProperty = "Id"))
	TArray<FHollowSettingsTabSpec> Tabs;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	FHollowSettingsTexts Texts;
};
