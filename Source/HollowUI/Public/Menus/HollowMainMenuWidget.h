#pragma once

#include "Foundation/HollowActivatableWidget.h"

#include "HollowMainMenuWidget.generated.h"

class UCommonButtonBase;
class UTextBlock;

/**
 * The front-end menu (L_Boot, S4-06) over the menu scene: play solo, host a co-op session (friends join through a
 * Steam invite), settings (UI-02, ADR-0052), quit. Both play buttons take UHollowSessionSubsystem::HostSession, the
 * same path as the console's Hollow.Session.Host. The footer shows the game's version and the language button
 * (LOC-01, ADR-0048), which cycles English / Russian at once and keeps the choice.
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
	UCommonButtonBase* GetLanguageButton() const { return LanguageButton; }
	UCommonButtonBase* GetSettingsButton() const { return SettingsButton; }
	UTextBlock* GetVersionText() const { return VersionText; }

	/** The footer's version line: "v" and the project's version (Project Settings > Description). */
	static FText VersionLabel();

protected:
	virtual void NativeOnInitialized() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> SoloButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> HostButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> QuitButton;

	/** Language toggle (LOC-01; optional so a tree from before it still compiles: create_ui_assets.py adds it). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> LanguageButton;

	/** UI-02 (optional so a tree from before it still compiles: create_ui_assets.py adds them). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> SettingsButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> VersionText;

private:
	void Host(bool bOnline);
	void Quit();
	void CycleLanguage();
	void OpenSettings();
};
