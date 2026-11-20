#include "Visor/HollowInteractionPromptWidget.h"

#include "Components/TextBlock.h"
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
	Refresh();
}

void UHollowInteractionPromptWidget::UnbindContext(UHollowVisorContext& InContext)
{
	InContext.GetPrompt()->OnChanged.Remove(PromptHandle);
}

void UHollowInteractionPromptWidget::Refresh()
{
	const UHollowPromptModel* Prompt = GetContext() ? GetContext()->GetPrompt() : nullptr;
	const bool bVisible = Prompt && Prompt->IsVisible();
	SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!bVisible)
	{
		return;
	}
	KeyText->SetText(Prompt->GetKeyLabel().IsEmpty() ? FText::GetEmpty() : FText::Format(KeyFormat, Prompt->GetKeyLabel()));
	PromptText->SetText(Prompt->GetPrompt());
}
