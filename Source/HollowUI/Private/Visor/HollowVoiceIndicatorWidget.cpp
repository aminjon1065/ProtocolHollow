#include "Visor/HollowVoiceIndicatorWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "ViewModels/HollowVisorContext.h"
#include "Visor/HollowVisorStyle.h"
#include "Voice/HollowVoiceComponent.h"
#include "Voice/HollowVoiceTalker.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowVoiceIndicatorWidget)

namespace HollowVoiceIndicatorPrivate
{
	constexpr float MaxLevel = 255.f;
}

void UHollowVoiceIndicatorWidget::BindContext(UHollowVisorContext& InContext)
{
	UHollowVoiceComponent* NewVoice = InContext.GetVoice();
	Voice = NewVoice;
	if (NewVoice)
	{
		ActivityHandle = NewVoice->OnVoiceActivityChanged.AddUObject(this, &ThisClass::HandleActivity);
		ModeHandle = NewVoice->OnVoiceModeChanged.AddUObject(this, &ThisClass::HandleMode);
		bTransmitting = NewVoice->IsTransmitting();
		Level = NewVoice->GetLevel();
	}
	TalkingHandle = UHollowVoiceTalker::OnTalkingChanged.AddUObject(this, &ThisClass::HandleTalking);
	Refresh();
}

void UHollowVoiceIndicatorWidget::UnbindContext(UHollowVisorContext& InContext)
{
	if (UHollowVoiceComponent* Old = Voice.Get())
	{
		Old->OnVoiceActivityChanged.Remove(ActivityHandle);
		Old->OnVoiceModeChanged.Remove(ModeHandle);
	}
	Voice.Reset();
	UHollowVoiceTalker::OnTalkingChanged.Remove(TalkingHandle);
	Talking.Reset();
}

void UHollowVoiceIndicatorWidget::HandleActivity(const bool bInTransmitting, const uint8 InLevel)
{
	bTransmitting = bInTransmitting;
	Level = InLevel;
	Refresh();
}

void UHollowVoiceIndicatorWidget::HandleMode(EHollowVoiceMode Mode)
{
	Refresh();
}

void UHollowVoiceIndicatorWidget::HandleTalking(const UHollowVoiceTalker* Talker, const bool bTalking)
{
	const APlayerController* Player = GetContext() ? GetContext()->GetPlayer() : nullptr;
	if (!Talker || (Player && Talker->GetTypedOuter<APlayerState>() == Player->PlayerState))
	{
		return;   // one's own voice is the mode line
	}
	Talking.RemoveAll([Talker](const TWeakObjectPtr<const UHollowVoiceTalker>& Each) { return !Each.IsValid() || Each.Get() == Talker; });
	if (bTalking)
	{
		Talking.Add(Talker);
	}
	Refresh();
}

FText UHollowVoiceIndicatorWidget::GetModeText() const
{
	const UHollowVisorStyle* Style = GetContext() ? GetContext()->GetStyle() : nullptr;
	const UHollowVoiceComponent* Source = Voice.Get();
	if (!Style || !Source)
	{
		return FText::GetEmpty();
	}
	const FText& Label = Style->GetVoiceModeLabel(Source->GetMode());
	return bTransmitting ? FText::Format(Style->TransmittingFormat, Label) : Label;
}

FText UHollowVoiceIndicatorWidget::GetTalkingText() const
{
	const UHollowVisorStyle* Style = GetContext() ? GetContext()->GetStyle() : nullptr;
	TArray<FText> Names;
	for (const TWeakObjectPtr<const UHollowVoiceTalker>& Talker : Talking)
	{
		if (const APlayerState* State = Talker.IsValid() ? Talker->GetTypedOuter<APlayerState>() : nullptr)
		{
			Names.Add(FText::FromString(State->GetPlayerName()));
		}
	}
	if (!Style || Names.IsEmpty())
	{
		return FText::GetEmpty();
	}
	return FText::Format(Style->TalkingFormat, FText::Join(INVTEXT(", "), Names));
}

void UHollowVoiceIndicatorWidget::Refresh()
{
	const UHollowVisorStyle* Style = GetContext() ? GetContext()->GetStyle() : nullptr;
	const UHollowVoiceComponent* Source = Voice.Get();
	SetVisibility(Source ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!Source)
	{
		return;
	}
	FLinearColor Colour = Style ? Style->TelemetryColor : FLinearColor::White;
	if (Style && Source->GetMode() == EHollowVoiceMode::Muted)
	{
		Colour = Style->MutedColor;
	}
	else if (Style && bTransmitting)
	{
		Colour = Style->TransmittingColor;
	}
	ModeText->SetText(GetModeText());
	ModeText->SetColorAndOpacity(FSlateColor(Colour));
	if (LevelBar)
	{
		LevelBar->SetVisibility(bTransmitting ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		LevelBar->SetPercent(static_cast<float>(Level) / HollowVoiceIndicatorPrivate::MaxLevel);
		LevelBar->SetFillColorAndOpacity(Colour);
	}
	if (TalkingText)
	{
		const FText Text = GetTalkingText();
		TalkingText->SetText(Text);
		TalkingText->SetVisibility(Text.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

FString UHollowVoiceIndicatorWidget::Describe() const
{
	return FString::Printf(TEXT("voice \"%s\" level %d, talking \"%s\""), *GetModeText().ToString(), Level, *GetTalkingText().ToString());
}
