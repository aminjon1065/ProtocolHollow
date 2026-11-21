#pragma once

#include "Hub/HollowHubScreenWidget.h"

#include "HollowWorkbenchWidget.generated.h"

class UHollowTabListWidget;

/** The workbench screen's tabs. */
UENUM()
enum class EHollowWorkbenchTab : uint8
{
	Craft,
	Shop,
	Upgrades
};

/**
 * The workbench / shop terminal's screen (W_Workbench, S5-04): crafting, the corporation's shop and the ship's
 * upgrades, one tab each (previous / next tab: Q / E, LB / RB, or the mouse). Every button is a transaction on this
 * player's own profile (ADR-0031, ADR-0032).
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowWorkbenchWidget : public UHollowHubScreenWidget
{
	GENERATED_BODY()

public:
	EHollowWorkbenchTab GetTab() const { return Tab; }

	/** Shows Tab (what its button and the tab actions do). */
	void SelectTab(EHollowWorkbenchTab InTab);

	/** The tab bar (tests). */
	UHollowTabListWidget* GetTabList() const { return Tabs; }

	static FName TabId(EHollowWorkbenchTab InTab);

protected:
	virtual void NativeOnActivated() override;
	virtual TArray<UPanelWidget*> GetLists() const override;
	virtual void RefreshRows(const FHollowProfileState& State, const FHollowEconomyTerms& Terms) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHollowTabListWidget> Tabs;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> RowList;

private:
	/** The tab bar loses its tabs whenever the screen is taken down: added again when shown. */
	void EnsureTabs();

	UFUNCTION()
	void HandleTabSelected(FName TabNameId);

	EHollowWorkbenchTab Tab = EHollowWorkbenchTab::Craft;
};
