#pragma once

#include "Hub/HollowHubScreenWidget.h"

#include "HollowLoadoutWidget.generated.h"

/**
 * The loadout locker's screen (W_Loadout, S5-04): the stash on the left (take into the loadout), the loadout on the
 * right (return to the stash, insure a slot within the limit the ship's upgrades raise). Transactions on this
 * player's own profile (ADR-0031). The class choice comes with S6-01.
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
};
