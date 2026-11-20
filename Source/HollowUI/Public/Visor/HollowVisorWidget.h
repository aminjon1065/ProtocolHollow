#pragma once

#include "Foundation/HollowActivatableWidget.h"
#include "Input/UIActionBindingHandle.h"

#include "HollowVisorWidget.generated.h"

/** The visor screen on the Game layer: game input, and the system menu action (Esc / Start). */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowVisorWidget : public UHollowActivatableWidget
{
	GENERATED_BODY()

public:
	explicit UHollowVisorWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

private:
	void OpenSystemMenu();

	FUIActionBindingHandle SystemMenuBinding;
};
