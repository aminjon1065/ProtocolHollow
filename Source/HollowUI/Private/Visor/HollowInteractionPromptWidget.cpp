#include "Visor/HollowInteractionPromptWidget.h"

#include "Components/TextBlock.h"
#include "ViewModels/HollowObjectiveModel.h"
#include "ViewModels/HollowPromptModel.h"
#include "ViewModels/HollowVisorContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowInteractionPromptWidget)

UHollowInteractionPromptWidget::UHollowInteractionPromptWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	KeyFormat = NSLOCTEXT("HollowUI", "PromptKeyFormat", "[{0}]");
	SetVisibility(ESlateVisibility::Collapsed);
}

void UHollowInteractionPromptWidget::BindContext(UHollowVisorContext& InContext)
{
	PromptHandle = InContext.GetPrompt()->OnChanged.AddUObject(this, &ThisClass::Refresh);
	if (UHollowObjectiveModel* Objective = InContext.GetObjective())
	{
		ObjectiveHandle = Objective->OnObjectiveChanged.AddUObject(this, &ThisClass::Refresh);   // a screen opened or closed
	}
	Refresh();
}

void UHollowInteractionPromptWidget::UnbindContext(UHollowVisorContext& InContext)
{
	InContext.GetPrompt()->OnChanged.Remove(PromptHandle);
	if (UHollowObjectiveModel* Objective = InContext.GetObjective())
	{
		Objective->OnObjectiveChanged.Remove(ObjectiveHandle);
	}
}

void UHollowInteractionPromptWidget::Refresh()
{
	const UHollowPromptModel* Prompt = GetContext() ? GetContext()->GetPrompt() : nullptr;
	const UHollowObjectiveModel* Objective = GetContext() ? GetContext()->GetObjective() : nullptr;
	const bool bVisible = Prompt && Prompt->IsVisible() && !(Objective && Objective->IsScreenOpen());
	SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!bVisible)
	{
		return;
	}
	KeyText->SetText(Prompt->GetKeyLabel().IsEmpty() ? FText::GetEmpty() : FText::Format(KeyFormat, Prompt->GetKeyLabel()));
	PromptText->SetText(Prompt->GetPrompt());
}
