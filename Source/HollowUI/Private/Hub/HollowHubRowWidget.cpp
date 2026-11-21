#include "Hub/HollowHubRowWidget.h"

#include "Components/TextBlock.h"
#include "Foundation/HollowButton.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowHubRowWidget)

void UHollowHubRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	for (int32 Index = 0; Index < HollowHubScreenRules::MaxActions; ++Index)
	{
		UHollowButton* Button = GetActionButton(Index);
		Button->OnClicked().AddUObject(this, &ThisClass::HandleClicked, Index);
		Button->OnFocusReceived().AddUObject(this, &ThisClass::HandleFocused, Index);
		Button->OnHovered().AddUObject(this, &ThisClass::HandleFocused, Index);
	}
}

UHollowButton* UHollowHubRowWidget::GetActionButton(const int32 Index) const
{
	switch (Index)
	{
	case 0:
		return FirstAction;
	case 1:
		return SecondAction;
	case 2:
		return ThirdAction;
	default:
		return nullptr;
	}
}

UHollowButton* UHollowHubRowWidget::GetFirstActionButton() const
{
	return Row.Actions.IsEmpty() ? nullptr : GetActionButton(0);
}

void UHollowHubRowWidget::SetRow(const FHollowHubRow& InRow)
{
	ensureMsgf(InRow.Actions.Num() <= HollowHubScreenRules::MaxActions, TEXT("%s: a hub row has at most %d buttons"), *GetName(), HollowHubScreenRules::MaxActions);
	Row = InRow;
	NameText->SetText(Row.Name);
	NameText->SetColorAndOpacity(FSlateColor(Row.bHeader ? HeaderColor : Row.bHighlighted ? HighlightColor : NameColor));
	DetailText->SetText(Row.Detail);
	DetailText->SetVisibility(Row.Detail.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	for (int32 Index = 0; Index < HollowHubScreenRules::MaxActions; ++Index)
	{
		UHollowButton* Button = GetActionButton(Index);
		if (!Row.Actions.IsValidIndex(Index))
		{
			Button->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}
		const FHollowHubAction& Action = Row.Actions[Index];
		Button->SetLabel(Action.Label);
		Button->SetIsInteractionEnabled(Action.bAvailable);   // greyed out, still focusable: the reason shows on focus
		Button->SetToolTipText(Action.bAvailable ? FText::GetEmpty() : Action.Reason);
		Button->SetVisibility(ESlateVisibility::Visible);
	}
}

int32 UHollowHubRowWidget::GetFocusedAction(APlayerController* Player) const
{
	for (int32 Index = 0; Index < Row.Actions.Num() && Index < HollowHubScreenRules::MaxActions; ++Index)
	{
		const UHollowButton* Button = GetActionButton(Index);
		// CommonUI buttons focus their inner Slate button. A player's focus needs its local player (none in tests).
		const bool bFocused = Player && Player->GetLocalPlayer() ? Button->HasUserFocus(Player) || Button->HasUserFocusedDescendants(Player)
			: Button->HasAnyUserFocus() || Button->HasFocusedDescendants();
		if (bFocused)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void UHollowHubRowWidget::HandleClicked(const int32 Index)
{
	if (Row.Actions.IsValidIndex(Index))
	{
		OnActionClicked.Broadcast(this, Index);
	}
}

void UHollowHubRowWidget::HandleFocused(const int32 Index)
{
	if (Row.Actions.IsValidIndex(Index))
	{
		OnActionFocused.Broadcast(this, Index);
	}
}
