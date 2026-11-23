#include "Visor/HollowWaypointMarkerWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Visor/HollowObjectiveRules.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowWaypointMarkerWidget)

void UHollowWaypointMarkerWidget::Show(const FText& Label, const FLinearColor& Color, const FHollowWaypointPlacement& Placement)
{
	LabelText->SetText(Label);
	LabelText->SetColorAndOpacity(FSlateColor(Color));
	Pip->SetColorAndOpacity(Color);
	Arrow->SetColorAndOpacity(Color);
	Pip->SetVisibility(Placement.bOnScreen ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	Arrow->SetVisibility(Placement.bOnScreen ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	Arrow->SetRenderTransformAngle(Placement.ArrowDegrees);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

FText UHollowWaypointMarkerWidget::GetLabel() const
{
	return LabelText->GetText();
}

bool UHollowWaypointMarkerWidget::IsArrowShown() const
{
	return Arrow->GetVisibility() != ESlateVisibility::Collapsed;
}
