#pragma once

#include "CommonTabListWidgetBase.h"

#include "HollowTabListWidget.generated.h"

class UPanelWidget;
class UTextBlock;
class UHollowVisorContext;

/**
 * The workbench's tab bar (W_HubTabList, S5-04): CommonUI's tab list with its buttons in a row. Previous / next tab
 * are UI actions (IA_UI_TabPrev / IA_UI_TabNext: Q / E, LB / RB, set on the instance); their keys show at the ends
 * of the bar for the device in use. Listens for them while it is constructed.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowTabListWidget : public UCommonTabListWidgetBase
{
	GENERATED_BODY()

public:
	/** Adds a tab whose button (TabButtonClass) reads Label. */
	bool AddTab(FName TabId, const FText& Label);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void HandleTabCreation_Implementation(FName TabNameID, UCommonButtonBase* TabButton) override;
	virtual void HandleTabRemoval_Implementation(FName TabNameID, UCommonButtonBase* TabButton) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> TabBox;

	/** The keys of the previous / next tab actions. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PreviousHint;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NextHint;

	/** Set on the instance (create_hub_ui_assets.py): a nested widget does not see class defaults set in the same run. */
	UPROPERTY(EditAnywhere, Category = "Tabs")
	TSubclassOf<UCommonButtonBase> TabButtonClass;

private:
	void RefreshHints();

	/** Labels of the tabs added, by id (lookups only). */
	TMap<FName, FText> Labels;

	/** The HUD's view of the player's input, for the hints (none in tests). */
	TWeakObjectPtr<UHollowVisorContext> HintContext;
	FDelegateHandle InputTypeHandle;
	FDelegateHandle KeysHandle;
};
