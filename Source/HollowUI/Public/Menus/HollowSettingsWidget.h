#pragma once

#include "Foundation/HollowActivatableWidget.h"

#include "HollowSettingsWidget.generated.h"

class UCommonButtonBase;
class UHollowGameUserSettings;
class UHollowSettingRowWidget;
class UHollowSettingsLayout;
class UHollowTabListWidget;
class UPanelWidget;
enum class EHollowSettingId : uint8;

/**
 * The settings screen on the Menu layer (W_Settings, UI-02, ADR-0052), opened from the main menu and the system menu:
 * the tabs of DA_SettingsLayout (previous / next tab: Q / E, LB / RB, or a click), one stepping row per setting. A step
 * applies at once and is kept (HollowSettingsRules); Back (Esc / B) or Close return to the menu it was opened from.
 * Event driven, no tick: the rows are filled when a tab shows and refilled after a step or a language change.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowSettingsWidget : public UHollowActivatableWidget
{
	GENERATED_BODY()

public:
	explicit UHollowSettingsWidget(const FObjectInitializer& ObjectInitializer);

	/** Shows a tab of the layout (and selects it in the tab bar once the screen is shown). */
	void SelectTab(FName TabId);
	FName GetSelectedTab() const { return SelectedTab; }

	/** The shown rows, in order (tests, Hollow.UI.Dump). */
	TArray<UHollowSettingRowWidget*> GetRowWidgets() const;
	UHollowSettingRowWidget* FindRow(EHollowSettingId Setting) const;

	UHollowTabListWidget* GetTabs() const { return Tabs; }
	UCommonButtonBase* GetCloseButton() const { return CloseButton; }
	const UHollowSettingsLayout* GetLayout() const { return Layout; }

	/** Tests: these settings instead of the engine's (they must outlive the screen). */
	void SetSettingsForTests(UHollowGameUserSettings* InSettings) { TestSettings = InSettings; }

	/** Public for tests (Back = Close: the screen goes, the menu under it shows again). */
	virtual bool NativeOnHandleBackAction() override;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHollowTabListWidget> Tabs;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> RowList;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> CloseButton;

	/** DA_SettingsLayout (create_ui_assets.py). */
	UPROPERTY(EditDefaultsOnly, Category = "Settings")
	TObjectPtr<UHollowSettingsLayout> Layout;

	/** W_SettingRow. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings")
	TSubclassOf<UHollowSettingRowWidget> RowClass;

	/** Room round each row in the list. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings")
	FMargin RowPadding;

private:
	UHollowGameUserSettings* GetSettings() const;
	void EnsureTabs();
	void ShowRows();
	void RefreshValues();
	void HandleStep(UHollowSettingRowWidget* Row, int32 Direction);

	UFUNCTION()
	void HandleTabSelected(FName TabId);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHollowSettingRowWidget>> Rows;

	/** Tests only (see SetSettingsForTests). */
	UPROPERTY(Transient)
	TObjectPtr<UHollowGameUserSettings> TestSettings;

	FName SelectedTab;
	FDelegateHandle LanguageHandle;
};
