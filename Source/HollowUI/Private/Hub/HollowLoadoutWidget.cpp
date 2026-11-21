#include "Hub/HollowLoadoutWidget.h"

#include "Components/PanelWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowLoadoutWidget)

TArray<UPanelWidget*> UHollowLoadoutWidget::GetLists() const
{
	return {StashList, LoadoutList};
}

void UHollowLoadoutWidget::RefreshRows(const FHollowProfileState& State, const FHollowEconomyTerms& Terms)
{
	FillList(StashList, HollowHubScreenRules::StashRows(State, Terms, GetNames()));
	FillList(LoadoutList, HollowHubScreenRules::LoadoutRows(State, Terms, GetNames()));
}
