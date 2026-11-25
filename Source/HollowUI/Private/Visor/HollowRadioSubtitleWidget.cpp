#include "Visor/HollowRadioSubtitleWidget.h"

#include "Components/TextBlock.h"
#include "ViewModels/HollowRadioModel.h"
#include "ViewModels/HollowVisorContext.h"
#include "Visor/HollowVisorStyle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowRadioSubtitleWidget)

void UHollowRadioSubtitleWidget::BindContext(UHollowVisorContext& InContext)
{
	Radio = InContext.GetRadio();
	if (UHollowRadioModel* Model = Radio.Get())
	{
		RadioHandle = Model->OnRadioChanged.AddUObject(this, &ThisClass::Refresh);
	}
	ClassHandle = InContext.OnClassChanged.AddUObject(this, &ThisClass::Refresh);   // the call sign in the class's colour
	Refresh();
}

void UHollowRadioSubtitleWidget::UnbindContext(UHollowVisorContext& InContext)
{
	if (UHollowRadioModel* Model = Radio.Get())
	{
		Model->OnRadioChanged.Remove(RadioHandle);
	}
	InContext.OnClassChanged.Remove(ClassHandle);
	Radio.Reset();
}

bool UHollowRadioSubtitleWidget::IsShown() const
{
	const UHollowRadioModel* Model = Radio.Get();
	return Model && Model->GetSubtitle().bShown;
}

FText UHollowRadioSubtitleWidget::GetSpeakerText() const
{
	const UHollowRadioModel* Model = Radio.Get();
	return Model ? Model->GetSubtitle().SpeakerName : FText::GetEmpty();
}

FText UHollowRadioSubtitleWidget::GetLineText() const
{
	const UHollowRadioModel* Model = Radio.Get();
	return Model ? Model->GetSubtitle().Text : FText::GetEmpty();
}

void UHollowRadioSubtitleWidget::Refresh()
{
	const bool bShown = IsShown();
	SetVisibility(bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	const UHollowVisorContext* Owner = GetContext();
	const UHollowVisorStyle* Style = Owner ? Owner->GetStyle() : nullptr;
	const bool bEcho = bShown && Radio->GetSubtitle().Speaker == EHollowRadioSpeaker::Echo;
	if (Style)
	{
		SpeakerText->SetColorAndOpacity(FSlateColor(bEcho ? Style->RadioEchoColor : Owner->GetTelemetryColor()));
		LineText->SetColorAndOpacity(FSlateColor(bEcho ? Style->RadioEchoColor : Style->RadioTextColor));
	}
	SpeakerText->SetText(GetSpeakerText());
	LineText->SetText(GetLineText());
}

FString UHollowRadioSubtitleWidget::Describe() const
{
	return IsShown() ? FString::Printf(TEXT("subtitle shown %s \"%s\""), *GetSpeakerText().ToString(), *GetLineText().ToString())
		: FString(TEXT("subtitle hidden"));
}
