#include "Hub/HollowTabListWidget.h"

#include "CommonButtonBase.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Foundation/HollowButton.h"
#include "Foundation/HollowHUD.h"
#include "ViewModels/HollowVisorContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowTabListWidget)

bool UHollowTabListWidget::AddTab(const FName TabId, const FText& Label)
{
	if (!ensureMsgf(TabButtonClass, TEXT("%s: TabButtonClass is not set (create_hub_ui_assets.py)"), *GetName()))
	{
		return false;
	}
	Labels.Add(TabId, Label);
	return RegisterTab(TabId, TabButtonClass, /*ContentWidget=*/nullptr);
}

void UHollowTabListWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetListeningForInput(true);   // previous / next tab while the screen is up (unlisted again on destruct)
	const AHollowHUD* HUD = AHollowHUD::Get(GetOwningPlayer());
	if (UHollowVisorContext* Context = HUD ? HUD->GetVisorContext() : nullptr)
	{
		HintContext = Context;
		InputTypeHandle = Context->OnInputTypeChanged.AddWeakLambda(this, [this](ECommonInputType) { RefreshHints(); });
		KeysHandle = Context->OnKeysChanged.AddUObject(this, &ThisClass::RefreshHints);
	}
	RefreshHints();
}

void UHollowTabListWidget::NativeDestruct()
{
	if (UHollowVisorContext* Context = HintContext.Get())
	{
		Context->OnInputTypeChanged.Remove(InputTypeHandle);
		Context->OnKeysChanged.Remove(KeysHandle);
	}
	HintContext.Reset();
	Super::NativeDestruct();   // removes every tab: the screen adds them again when it is shown next
}

void UHollowTabListWidget::HandleTabCreation_Implementation(const FName TabNameID, UCommonButtonBase* TabButton)
{
	if (UHollowButton* Button = Cast<UHollowButton>(TabButton))
	{
		Button->SetLabel(Labels.FindRef(TabNameID));
	}
	TabBox->AddChild(TabButton);
}

void UHollowTabListWidget::HandleTabRemoval_Implementation(const FName TabNameID, UCommonButtonBase* TabButton)
{
	if (TabButton)
	{
		TabButton->RemoveFromParent();
	}
}

void UHollowTabListWidget::RefreshHints()
{
	const UHollowVisorContext* Context = HintContext.Get();
	auto Show = [Context](UTextBlock* Hint, const UInputAction* Action)
	{
		if (!Hint)
		{
			return;
		}
		const FText Key = Context && Action ? Context->GetKeyLabel(Action) : FText::GetEmpty();
		Hint->SetText(Key);
		Hint->SetVisibility(Key.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	};
	Show(PreviousHint, PreviousTabEnhancedInputAction);
	Show(NextHint, NextTabEnhancedInputAction);
}
