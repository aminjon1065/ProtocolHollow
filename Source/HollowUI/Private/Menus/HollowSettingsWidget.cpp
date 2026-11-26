#include "Menus/HollowSettingsWidget.h"

#include "CommonButtonBase.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBoxSlot.h"
#include "Hub/HollowTabListWidget.h"
#include "HollowUILog.h"
#include "Localization/HollowLanguage.h"
#include "Menus/HollowSettingRowWidget.h"
#include "Menus/HollowSettingsLayout.h"
#include "Menus/HollowSettingsRules.h"
#include "Settings/HollowGameUserSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowSettingsWidget)

UHollowSettingsWidget::UHollowSettingsWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InputMode = EHollowWidgetInputMode::Menu;
	bIsBackHandler = true;   // Back deactivates it: the menu under it shows again
}

UHollowGameUserSettings* UHollowSettingsWidget::GetSettings() const
{
	return TestSettings ? TestSettings.Get() : UHollowGameUserSettings::Get();
}

void UHollowSettingsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	CloseButton->OnClicked().AddUObject(this, &ThisClass::DeactivateWidget);
	Tabs->OnTabSelected.AddUniqueDynamic(this, &ThisClass::HandleTabSelected);
}

void UHollowSettingsWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	EnsureTabs();
	if (SelectedTab.IsNone() && Layout && !Layout->Tabs.IsEmpty())
	{
		SelectedTab = Layout->Tabs[0].Id;
	}
	Tabs->SelectTabByID(SelectedTab, /*bSuppressClickFeedback=*/true);
	ShowRows();
	LanguageHandle = HollowLanguage::OnApplied().AddUObject(this, &ThisClass::RefreshValues);
}

void UHollowSettingsWidget::NativeOnDeactivated()
{
	HollowLanguage::OnApplied().Remove(LanguageHandle);
	LanguageHandle.Reset();
	Super::NativeOnDeactivated();
}

bool UHollowSettingsWidget::NativeOnHandleBackAction()
{
	return Super::NativeOnHandleBackAction();   // CommonUI's: a back handler deactivates
}

void UHollowSettingsWidget::EnsureTabs()
{
	if (!Layout || Tabs->GetTabCount() > 0)
	{
		return;   // the tab bar removes its tabs when destructed: added again when shown next
	}
	for (const FHollowSettingsTabSpec& Tab : Layout->Tabs)
	{
		Tabs->AddTab(Tab.Id, Tab.Label);
	}
}

void UHollowSettingsWidget::SelectTab(const FName TabId)
{
	if (!Tabs->SelectTabByID(TabId))
	{
		HandleTabSelected(TabId);   // no tab bar yet (not shown): the rows follow anyway
	}
}

void UHollowSettingsWidget::HandleTabSelected(const FName TabId)
{
	if (TabId == SelectedTab && !Rows.IsEmpty())
	{
		return;
	}
	SelectedTab = TabId;
	UE_LOG(LogHollowUI, Log, TEXT("%s: tab %s"), *GetName(), *TabId.ToString());
	ShowRows();
	RequestRefreshFocus();   // the first row of the new tab, unless the focused row is still shown
}

void UHollowSettingsWidget::ShowRows()
{
	const FHollowSettingsTabSpec* Tab = Layout ? Layout->FindTab(SelectedTab) : nullptr;
	const int32 Count = Tab ? Tab->Rows.Num() : 0;
	if (!RowClass && Count > 0)
	{
		UE_LOG(LogHollowUI, Error, TEXT("%s: RowClass is not set (create_ui_assets.py)."), *GetName());
		return;
	}
	while (Rows.Num() < Count)
	{
		UHollowSettingRowWidget* Row = CreateWidget<UHollowSettingRowWidget>(this, RowClass);
		Row->OnStep.AddUObject(this, &ThisClass::HandleStep);
		if (UVerticalBoxSlot* RowSlot = Cast<UVerticalBoxSlot>(RowList->AddChild(Row)))
		{
			RowSlot->SetPadding(RowPadding);
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		Rows.Add(Row);
	}
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		UHollowSettingRowWidget* Row = Rows[Index];
		if (Index < Count)
		{
			Row->SetSetting(Tab->Rows[Index].Setting, Tab->Rows[Index].Label);
			Row->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			Row->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	RefreshValues();
}

void UHollowSettingsWidget::RefreshValues()
{
	const UHollowGameUserSettings* Settings = GetSettings();
	if (!Settings || !Layout)
	{
		return;
	}
	for (UHollowSettingRowWidget* Row : GetRowWidgets())
	{
		Row->SetValue(HollowSettingsRules::ValueText(Row->GetSetting(), *Settings, Layout->Texts));
	}
}

void UHollowSettingsWidget::HandleStep(UHollowSettingRowWidget* Row, const int32 Direction)
{
	UHollowGameUserSettings* Settings = GetSettings();
	if (!Row || !Settings)
	{
		return;
	}
	if (HollowSettingsRules::Step(Row->GetSetting(), *Settings, Direction, GetOwningPlayer()))
	{
		UE_LOG(LogHollowUI, Log, TEXT("%s: %s -> %s"), *GetName(), *UEnum::GetValueAsString(Row->GetSetting()),
			*HollowSettingsRules::ValueText(Row->GetSetting(), *Settings, Layout->Texts).ToString());
	}
	RefreshValues();
}

TArray<UHollowSettingRowWidget*> UHollowSettingsWidget::GetRowWidgets() const
{
	TArray<UHollowSettingRowWidget*> Shown;
	for (UHollowSettingRowWidget* Row : Rows)
	{
		if (Row && Row->GetVisibility() != ESlateVisibility::Collapsed)
		{
			Shown.Add(Row);
		}
	}
	return Shown;
}

UHollowSettingRowWidget* UHollowSettingsWidget::FindRow(const EHollowSettingId Setting) const
{
	for (UHollowSettingRowWidget* Row : GetRowWidgets())
	{
		if (Row->GetSetting() == Setting)
		{
			return Row;
		}
	}
	return nullptr;
}

UWidget* UHollowSettingsWidget::NativeGetDesiredFocusTarget() const
{
	const TArray<UHollowSettingRowWidget*> Shown = GetRowWidgets();
	for (UHollowSettingRowWidget* Row : Shown)
	{
		if (Row->HasAnyUserFocus() || Row->HasFocusedDescendants())
		{
			return Row;   // a switch between keyboard and gamepad keeps the focus where it is (UI-H2)
		}
	}
	return Shown.IsEmpty() ? static_cast<UWidget*>(CloseButton) : Shown[0];
}
