#include "Hub/HollowWorkbenchWidget.h"

#include "Components/PanelWidget.h"
#include "Foundation/HollowUISettings.h"
#include "HollowUILog.h"
#include "Hub/HollowTabListWidget.h"
#include "Util/HollowEnumUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowWorkbenchWidget)

#define LOCTEXT_NAMESPACE "HollowHubScreens"

namespace HollowWorkbench
{
	constexpr EHollowWorkbenchTab AllTabs[] = {EHollowWorkbenchTab::Craft, EHollowWorkbenchTab::Shop, EHollowWorkbenchTab::Upgrades};

	FText TabLabel(const EHollowWorkbenchTab Tab)
	{
		switch (Tab)
		{
		case EHollowWorkbenchTab::Craft:
			return LOCTEXT("CraftTab", "Craft");
		case EHollowWorkbenchTab::Shop:
			return LOCTEXT("ShopTab", "Shop");
		case EHollowWorkbenchTab::Upgrades:
			return LOCTEXT("UpgradesTab", "Ship upgrades");
		}
		return FText::GetEmpty();
	}
}

FName UHollowWorkbenchWidget::TabId(const EHollowWorkbenchTab InTab)
{
	return FName(*HollowEnumToString(InTab));
}

void UHollowWorkbenchWidget::EnsureTabs()
{
	if (Tabs->GetTabCount() > 0)
	{
		return;
	}
	Tabs->OnTabSelected.AddUniqueDynamic(this, &ThisClass::HandleTabSelected);
	for (const EHollowWorkbenchTab Each : HollowWorkbench::AllTabs)
	{
		Tabs->AddTab(TabId(Each), HollowWorkbench::TabLabel(Each));
	}
}

void UHollowWorkbenchWidget::NativeOnActivated()
{
	EnsureTabs();
	Tabs->SelectTabByID(TabId(Tab), /*bSuppressClickFeedback=*/true);
	Super::NativeOnActivated();
}

void UHollowWorkbenchWidget::SelectTab(const EHollowWorkbenchTab InTab)
{
	if (!Tabs->SelectTabByID(TabId(InTab)))
	{
		HandleTabSelected(TabId(InTab));   // no tab bar yet (not shown): the rows follow anyway
	}
}

void UHollowWorkbenchWidget::HandleTabSelected(const FName TabNameId)
{
	for (const EHollowWorkbenchTab Each : HollowWorkbench::AllTabs)
	{
		if (TabId(Each) == TabNameId && Tab != Each)
		{
			Tab = Each;
			UE_LOG(LogHollowUI, Log, TEXT("%s: tab %s"), *GetName(), *TabNameId.ToString());
			SetMessage(FText::GetEmpty());
			if (IsActivated())
			{
				Refresh();
			}
		}
	}
}

TArray<UPanelWidget*> UHollowWorkbenchWidget::GetLists() const
{
	return {RowList};
}

void UHollowWorkbenchWidget::RefreshRows(const FHollowProfileState& State, const FHollowEconomyTerms& Terms)
{
	switch (Tab)
	{
	case EHollowWorkbenchTab::Craft:
		FillList(RowList, HollowHubScreenRules::CraftRows(State, Terms, GetNames()));
		break;
	case EHollowWorkbenchTab::Shop:
		FillList(RowList, HollowHubScreenRules::ShopRows(State, Terms, GetNames(), GetDefault<UHollowUISettings>()->ShopBulkCount));
		break;
	case EHollowWorkbenchTab::Upgrades:
		FillList(RowList, HollowHubScreenRules::UpgradeRows(State, Terms, GetNames()));
		break;
	}
}

#undef LOCTEXT_NAMESPACE
