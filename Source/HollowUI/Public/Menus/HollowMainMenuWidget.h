#pragma once

#include "Foundation/HollowActivatableWidget.h"

#include "HollowMainMenuWidget.generated.h"

class UCommonButtonBase;

/**
 * The front-end menu (L_Boot, S4-06): play solo, host a co-op session (friends join through a Steam invite), quit.
 * Both play buttons take UHollowSessionSubsystem::HostSession, the same path as the console's Hollow.Session.Host.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowMainMenuWidget : public UHollowActivatableWidget
{
	GENERATED_BODY()

public:
	explicit UHollowMainMenuWidget(const FObjectInitializer& ObjectInitializer);

	UCommonButtonBase* GetSoloButton() const { return SoloButton; }
	UCommonButtonBase* GetHostButton() const { return HostButton; }
	UCommonButtonBase* GetQuitButton() const { return QuitButton; }

protected:
	virtual void NativeOnInitialized() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> SoloButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> HostButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> QuitButton;

private:
	void Host(bool bOnline);
	void Quit();
};
