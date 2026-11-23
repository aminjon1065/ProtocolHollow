#pragma once

#include "Engine/DeveloperSettings.h"

#include "HollowUISettings.generated.h"

class UCommonActivatableWidget;
class UHollowControlsHintSet;
class UHollowInputGlyphSet;
class UHollowDebriefWidget;
class UHollowHubScreenWidget;
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

	/** The hub terminals' screens on the Menu layer (S5-04, ADR-0033), one per terminal kind. */
	UPROPERTY(Config, EditAnywhere, Category = "Hub")
	TSoftClassPtr<UHollowHubScreenWidget> WorkbenchScreenClass;

	UPROPERTY(Config, EditAnywhere, Category = "Hub")
	TSoftClassPtr<UHollowHubScreenWidget> LoadoutScreenClass;

	UPROPERTY(Config, EditAnywhere, Category = "Hub")
	TSoftClassPtr<UHollowHubScreenWidget> ContractScreenClass;

	/** The shop's second buy button: Buy this many at once. */
	UPROPERTY(Config, EditAnywhere, Category = "Hub", meta = (ClampMin = "2"))
	int32 ShopBulkCount = 0;

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

	/** UX-01 (ADR-0042): folds and unfolds the visor's controls cheat sheet (H / View). */
	UPROPERTY(Config, EditAnywhere, Category = "Input")
	TSoftObjectPtr<UInputAction> ControlsHintAction;

	/** Tiers, telemetry texts and colours of the visor (S4-08, DA_VisorStyle). */
	UPROPERTY(Config, EditAnywhere, Category = "Visor")
	TSoftObjectPtr<UHollowVisorStyle> VisorStyle;

	/** UX-01: the rows of the visor's controls cheat sheet (DA_ControlsHint). */
	UPROPERTY(Config, EditAnywhere, Category = "Visor")
	TSoftObjectPtr<UHollowControlsHintSet> ControlsHint;

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
