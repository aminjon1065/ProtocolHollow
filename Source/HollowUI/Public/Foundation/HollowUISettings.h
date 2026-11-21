#pragma once

#include "Engine/DeveloperSettings.h"

#include "HollowUISettings.generated.h"

class UCommonActivatableWidget;
class UHollowInputGlyphSet;
class UHollowDebriefWidget;
class UHollowPrimaryLayout;
class UHollowVisorStyle;
class UInputAction;
class UInputMappingContext;

/** UI wiring and visor tuning (Project Settings > Game > Hollow UI, DefaultGame.ini). Widget classes are WBPs (layout). */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Hollow UI"))
class HOLLOWUI_API UHollowUISettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Layout")
	TSoftClassPtr<UHollowPrimaryLayout> PrimaryLayoutClass;

	/** Pushed on the Game layer for every local player. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout")
	TSoftClassPtr<UCommonActivatableWidget> VisorClass;

	UPROPERTY(Config, EditAnywhere, Category = "Layout")
	TSoftClassPtr<UCommonActivatableWidget> SystemMenuClass;

	/** On the front-end map instead of the visor (S4-06): solo, host co-op, quit. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout")
	TSoftClassPtr<UCommonActivatableWidget> MainMenuClass;

	/** Raid debrief on the Menu layer (S3-03). */
	UPROPERTY(Config, EditAnywhere, Category = "Layout")
	TSoftClassPtr<UHollowDebriefWidget> DebriefClass;

	/** UI actions (confirm, back, system menu): CommonUI finds their keys in applied mapping contexts. */
	UPROPERTY(Config, EditAnywhere, Category = "Input")
	TSoftObjectPtr<UInputMappingContext> UIMappingContext;

	UPROPERTY(Config, EditAnywhere, Category = "Input")
	int32 UIMappingPriority = 0;

	UPROPERTY(Config, EditAnywhere, Category = "Input")
	TSoftObjectPtr<UInputAction> SystemMenuAction;

	/** Its key is shown in the interaction prompt. */
	UPROPERTY(Config, EditAnywhere, Category = "Input")
	TSoftObjectPtr<UInputAction> InteractAction;

	/** Its key is shown on the visor's lamp line (S4-08). */
	UPROPERTY(Config, EditAnywhere, Category = "Input")
	TSoftObjectPtr<UInputAction> HelmetLampAction;

	UPROPERTY(Config, EditAnywhere, Category = "Input")
	TSoftObjectPtr<UHollowInputGlyphSet> InputGlyphs;

	/** Tiers, telemetry texts and colours of the visor (S4-08, DA_VisorStyle). */
	UPROPERTY(Config, EditAnywhere, Category = "Visor")
	TSoftObjectPtr<UHollowVisorStyle> VisorStyle;

	/** How often the visor re-traces the active tool's target. */
	UPROPERTY(Config, EditAnywhere, Category = "Visor", meta = (ClampMin = "1", Units = "Hertz"))
	float TargetProbeRateHz = 0.f;

	/** How often the visor's raid line redraws a countdown (only while a phase is timed). */
	UPROPERTY(Config, EditAnywhere, Category = "Visor", meta = (ClampMin = "1", Units = "Hertz"))
	float RaidStatusRefreshRateHz = 1.f;

	/** The target bar stays this long after the tool stops or leaves the target. */
	UPROPERTY(Config, EditAnywhere, Category = "Visor", meta = (ClampMin = "0", Units = "Seconds"))
	float TargetLingerSeconds = 0.f;
};
