#include "Menus/HollowSettingRowWidget.h"

#include "Components/TextBlock.h"
#include "Input/Events.h"
#include "Input/NavigationReply.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowSettingRowWidget)

namespace HollowSettingRow
{
	/** Single angle quotation marks (U+2039, U+203A): in the game's font (Roboto), every language. */
	const TCHAR* const DecreaseGlyph = TEXT("\u2039");
	const TCHAR* const IncreaseGlyph = TEXT("\u203A");

	bool IsUnder(const UWidget* Widget, const FVector2D& ScreenPosition)
	{
		return Widget && Widget->IsVisible() && Widget->GetCachedGeometry().IsUnderLocation(ScreenPosition);
	}
}

void UHollowSettingRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	OnClicked().AddUObject(this, &ThisClass::HandleClicked);
	if (DecreaseHint)
	{
		DecreaseHint->SetText(FText::AsCultureInvariant(HollowSettingRow::DecreaseGlyph));
	}
	if (IncreaseHint)
	{
		IncreaseHint->SetText(FText::AsCultureInvariant(HollowSettingRow::IncreaseGlyph));
	}
}

void UHollowSettingRowWidget::SetSetting(const EHollowSettingId InSetting, const FText& InLabel)
{
	Setting = InSetting;
	SetLabel(InLabel);
}

void UHollowSettingRowWidget::SetValue(const FText& Value)
{
	ValueText->SetText(Value);
}

FText UHollowSettingRowWidget::GetValue() const
{
	return ValueText->GetText();
}

void UHollowSettingRowWidget::Step(const int32 Direction)
{
	if (Direction != 0)
	{
		OnStep.Broadcast(this, Direction > 0 ? 1 : -1);
	}
}

FNavigationReply UHollowSettingRowWidget::NativeOnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent, const FNavigationReply& InDefaultReply)
{
	const EUINavigation Direction = InNavigationEvent.GetNavigationType();
	if (Direction == EUINavigation::Left || Direction == EUINavigation::Right)
	{
		Step(Direction == EUINavigation::Right ? 1 : -1);
		return FNavigationReply::Stop();   // the focus stays on the row
	}
	return Super::NativeOnNavigation(MyGeometry, InNavigationEvent, InDefaultReply);
}

FReply UHollowSettingRowWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// The press itself goes on to the button (its sound, its click): only remember which arrow it went down on.
	ClickDirection = HollowSettingRow::IsUnder(DecreaseHint, InMouseEvent.GetScreenSpacePosition()) ? -1 : 1;
	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UHollowSettingRowWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (FMath::IsNearlyZero(InMouseEvent.GetWheelDelta()))
	{
		return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
	}
	Step(InMouseEvent.GetWheelDelta() > 0.f ? 1 : -1);
	return FReply::Handled();
}

void UHollowSettingRowWidget::HandleClicked()
{
	const int32 Direction = ClickDirection;
	ClickDirection = 1;   // a keyboard or gamepad press next steps up
	Step(Direction);
}
