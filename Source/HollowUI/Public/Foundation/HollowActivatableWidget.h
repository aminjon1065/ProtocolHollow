#pragma once

#include "CommonActivatableWidget.h"

#include "HollowActivatableWidget.generated.h"

/** Input a widget asks CommonUI for while it is the active one. */
UENUM(BlueprintType)
enum class EHollowWidgetInputMode : uint8
{
	/** Keep the current input config. */
	Default,
	/** Gameplay input, captured mouse (the visor). */
	Game,
	/** UI input only, free cursor, gamepad navigation (menus). */
	Menu,
	/** Both: panels that keep the pawn controllable. */
	GameAndMenu
};

/** Base of every Hollow screen on a layout layer (ADR-0013): declares its input mode as data. */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowActivatableWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	EHollowWidgetInputMode InputMode = EHollowWidgetInputMode::Default;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	EMouseCaptureMode GameMouseCaptureMode = EMouseCaptureMode::CapturePermanently;
};
