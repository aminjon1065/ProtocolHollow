#include "Hub/HollowBadgeWidget.h"

#include "Components/Border.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Foundation/HollowUIStyle.h"
#include "HollowUILog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowBadgeWidget)

void UHollowBadgeWidget::SetBadge(const FHollowHubBadge& Badge)
{
	Kind = Badge.Kind;
	Label->SetText(Badge.Text);
	const UHollowUIStyle* Style = UHollowUIStyle::Get();
	if (const FHollowBadgeLook* Look = Style ? Style->FindBadge(Badge.Kind) : nullptr)
	{
		FSlateBrush Brush = Frame->Background;   // the rounded glass of hollow_ui_style.py, recoloured
		Brush.TintColor = FSlateColor(Look->Fill);
		Brush.OutlineSettings.Color = FSlateColor(Look->Frame);
		Frame->SetBrush(Brush);
		Label->SetColorAndOpacity(FSlateColor(Look->Text));
	}
}

FText UHollowBadgeWidget::GetText() const
{
	return Label->GetText();
}

void UHollowBadgeWidget::FillBox(UPanelWidget* Box, const TConstArrayView<FHollowHubBadge> Badges, UUserWidget& Owner)
{
	if (!Box)
	{
		return;
	}
	const UHollowUIStyle* Style = UHollowUIStyle::Get();
	const TSubclassOf<UHollowBadgeWidget> BadgeClass = Style ? Style->BadgeClass.LoadSynchronous() : nullptr;
	if (!BadgeClass && !Badges.IsEmpty())
	{
		UE_LOG(LogHollowUI, Warning, TEXT("%s: no badge class (DA_UIStyle)."), *Owner.GetName());
		return;
	}
	for (int32 Index = 0; Index < Badges.Num(); ++Index)
	{
		UHollowBadgeWidget* Badge = Index < Box->GetChildrenCount() ? Cast<UHollowBadgeWidget>(Box->GetChildAt(Index)) : nullptr;
		if (!Badge)
		{
			Badge = CreateWidget<UHollowBadgeWidget>(&Owner, BadgeClass);
			Box->AddChild(Badge);
		}
		Badge->SetBadge(Badges[Index]);
		Badge->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	for (int32 Index = Badges.Num(); Index < Box->GetChildrenCount(); ++Index)
	{
		Box->GetChildAt(Index)->SetVisibility(ESlateVisibility::Collapsed);
	}
	Box->SetVisibility(Badges.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}
