#include "Visor/HollowObjectiveWidget.h"

#include "Components/TextBlock.h"
#include "ViewModels/HollowObjectiveModel.h"
#include "ViewModels/HollowVisorContext.h"
#include "Visor/HollowVisorStyle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowObjectiveWidget)

void UHollowObjectiveWidget::BindContext(UHollowVisorContext& InContext)
{
	Objective = InContext.GetObjective();
	if (UHollowObjectiveModel* Model = Objective.Get())
	{
		ObjectiveHandle = Model->OnObjectiveChanged.AddUObject(this, &ThisClass::Refresh);
	}
	ClassHandle = InContext.OnClassChanged.AddUObject(this, &ThisClass::Refresh);   // the title in the class's colour
	Refresh();
}

void UHollowObjectiveWidget::UnbindContext(UHollowVisorContext& InContext)
{
	if (UHollowObjectiveModel* Model = Objective.Get())
	{
		Model->OnObjectiveChanged.Remove(ObjectiveHandle);
	}
	InContext.OnClassChanged.Remove(ClassHandle);
	Objective.Reset();
}

FText UHollowObjectiveWidget::GetObjectiveText() const
{
	const UHollowObjectiveModel* Model = Objective.Get();
	return Model ? Model->GetObjective().Text : FText::GetEmpty();
}

bool UHollowObjectiveWidget::IsShown() const
{
	return !GetObjectiveText().IsEmpty();
}

void UHollowObjectiveWidget::Refresh()
{
	SetVisibility(IsShown() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	const UHollowVisorContext* Owner = GetContext();
	const UHollowVisorStyle* Style = Owner ? Owner->GetStyle() : nullptr;
	if (Style)
	{
		TitleText->SetText(Style->ObjectiveTitle);
		TitleText->SetColorAndOpacity(FSlateColor(Owner->GetTelemetryColor()));
		ObjectiveText->SetColorAndOpacity(FSlateColor(Style->ObjectiveColor));
	}
	ObjectiveText->SetText(GetObjectiveText());
}

FString UHollowObjectiveWidget::Describe() const
{
	return FString::Printf(TEXT("tracker %s \"%s\""), IsShown() ? TEXT("shown") : TEXT("hidden"), *GetObjectiveText().ToString());
}
