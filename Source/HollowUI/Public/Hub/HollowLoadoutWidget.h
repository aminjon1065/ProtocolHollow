#pragma once

#include "Hub/HollowHubScreenWidget.h"

#include "HollowLoadoutWidget.generated.h"

/**
 * The loadout locker's screen (W_Loadout, S5-04): the stash on the left (take into the loadout), the loadout on the
 * right (return to the stash, insure a slot within the limit the ship's upgrades raise), the player classes above them
 * (S6-01, ADR-0034: a class brings its own loadout back). Transactions on this player's own profile (ADR-0031).
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowLoadoutWidget : public UHollowHubScreenWidget
{
	GENERATED_BODY()

protected:
	virtual TArray<UPanelWidget*> GetLists() const override;
	virtual void RefreshRows(const FHollowProfileState& State, const FHollowEconomyTerms& Terms) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> StashList;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> LoadoutList;

	/** The classes (S6-01; optional so a tree from before them still compiles: create_hub_ui_assets.py adds it). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> ClassList;
};
