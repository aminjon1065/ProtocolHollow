#pragma once

#include "Foundation/HollowActivatableWidget.h"

#include "HollowSystemMenuWidget.generated.h"

class UCommonButtonBase;

/**
 * System menu on the Menu layer (Esc / Start): resume or quit. Never pauses the game, solo or co-op (parity,
 * CLAUDE.md sec. 1). Back (Esc / B) closes it; gamepad focus starts on Resume.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowSystemMenuWidget : public UHollowActivatableWidget
{
	GENERATED_BODY()

public:
	explicit UHollowSystemMenuWidget(const FObjectInitializer& ObjectInitializer);

	UCommonButtonBase* GetResumeButton() const { return ResumeButton; }

protected:
	virtual void NativeOnInitialized() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> ResumeButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> QuitButton;

private:
	void Resume();
	void Quit();
};
