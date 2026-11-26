#include "Visor/HollowAcousticAlertWidget.h"

#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Framework/HollowPlayerController.h"
#include "TimerManager.h"
#include "ViewModels/HollowVisorContext.h"
#include "Visor/HollowVisorStyle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowAcousticAlertWidget)

void UHollowAcousticAlertWidget::BindContext(UHollowVisorContext& InContext)
{
	Controller = Cast<AHollowPlayerController>(InContext.GetPlayer());
	if (AHollowPlayerController* Player = Controller.Get())
	{
		AlertHandle = Player->OnAcousticAlert.AddUObject(this, &ThisClass::ShowAlert);
	}
	SetVisibility(bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UHollowAcousticAlertWidget::UnbindContext(UHollowVisorContext& InContext)
{
	if (AHollowPlayerController* Player = Controller.Get())
	{
		Player->OnAcousticAlert.Remove(AlertHandle);
	}
	Controller.Reset();
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimer);
	}
}

void UHollowAcousticAlertWidget::ShowAlert(const FGameplayTag& NoiseTag)
{
	const UHollowVisorContext* Owner = GetContext();
	const UHollowVisorStyle* Style = Owner ? Owner->GetStyle() : nullptr;
	if (!Style)
	{
		return;
	}
	LastNoise = NoiseTag;
	AlertText->SetText(Style->AcousticAlertText);
	AlertText->SetColorAndOpacity(FSlateColor(Style->AcousticAlertColor));
	bShown = true;
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(HideTimer, this, &ThisClass::Hide, FMath::Max(Style->AcousticAlertSeconds, 0.1f), /*bLoop=*/false);
	}
}

void UHollowAcousticAlertWidget::Hide()
{
	bShown = false;
	SetVisibility(ESlateVisibility::Collapsed);
}

FText UHollowAcousticAlertWidget::GetAlertText() const
{
	return AlertText->GetText();
}

FString UHollowAcousticAlertWidget::Describe() const
{
	return bShown ? FString::Printf(TEXT("acoustic alert shown \"%s\" (%s)"), *GetAlertText().ToString(), *LastNoise.ToString())
		: FString(TEXT("acoustic alert hidden"));
}
