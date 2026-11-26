#pragma once

#include "Foundation/HollowActivatableWidget.h"
#include "Input/UIActionBindingHandle.h"

#include "HollowVisorWidget.generated.h"

class UHollowWaypointLayerWidget;

/** The visor screen on the Game layer: game input, the system menu action (Esc / Start) and the cheat sheet's (H / View). */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowVisorWidget : public UHollowActivatableWidget
{
	GENERATED_BODY()

public:
	explicit UHollowVisorWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	/** POLISH-01: the waypoint layer the controls key reveals (immersive waypoints). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHollowWaypointLayerWidget> Waypoints;

private:
	void OpenSystemMenu();
	/**
	 * UX-01: H / View folds or unfolds the controls cheat sheet (kept in the game's settings). POLISH-01: and shows
	 * immersive waypoints for a while in a raid.
	 */
	void ToggleControlsHint();

	FUIActionBindingHandle SystemMenuBinding;
	FUIActionBindingHandle ControlsHintBinding;
};
