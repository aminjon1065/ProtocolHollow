#include "Visor/HollowTargetStatusWidget.h"

#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "ViewModels/HollowToolTargetModel.h"
#include "ViewModels/HollowVisorContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowTargetStatusWidget)

namespace HollowTargetStatusPrivate
{
	/** MarkerRow layout: spacer, marker, spacer. */
	constexpr int32 MarkerRowChildren = 3;

	void SetFill(UWidget* Child, const float Fill)
	{
		if (UHorizontalBoxSlot* Slot = Child ? Cast<UHorizontalBoxSlot>(Child->Slot) : nullptr)
		{
			FSlateChildSize Size(ESlateSizeRule::Fill);
			Size.Value = Fill;
			Slot->SetSize(Size);
		}
	}
}

UHollowTargetStatusWidget::UHollowTargetStatusWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	LabelFormat = NSLOCTEXT("HollowUI", "TargetStatusFormat", "{0} {1}");
	SetVisibility(ESlateVisibility::Collapsed);
}

void UHollowTargetStatusWidget::BindContext(UHollowVisorContext& InContext)
{
	TargetHandle = InContext.GetToolTarget()->OnChanged.AddUObject(this, &ThisClass::Refresh);
	Refresh();
}

void UHollowTargetStatusWidget::UnbindContext(UHollowVisorContext& InContext)
{
	InContext.GetToolTarget()->OnChanged.Remove(TargetHandle);
}

void UHollowTargetStatusWidget::Refresh()
{
	using namespace HollowTargetStatusPrivate;
	const UHollowToolTargetModel* Model = GetContext() ? GetContext()->GetToolTarget() : nullptr;
	const bool bVisible = Model && Model->IsVisible();
	SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!bVisible)
	{
		return;
	}
	const FHollowTargetStatus& Status = Model->GetStatus();
	Bar->SetPercent(Status.Value);
	if (LabelText)
	{
		LabelText->SetText(FText::Format(LabelFormat, Model->GetActionLabel(), FText::AsPercent(Status.Value)));
	}
	if (MarkerRow && MarkerRow->GetChildrenCount() == MarkerRowChildren)
	{
		const bool bMarker = Status.Marker >= 0.f;
		MarkerRow->SetVisibility(bMarker ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		SetFill(MarkerRow->GetChildAt(0), FMath::Clamp(Status.Marker, 0.f, 1.f));
		SetFill(MarkerRow->GetChildAt(MarkerRowChildren - 1), 1.f - FMath::Clamp(Status.Marker, 0.f, 1.f));
	}
}
