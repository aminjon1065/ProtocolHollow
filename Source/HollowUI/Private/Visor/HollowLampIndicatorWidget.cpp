#include "Visor/HollowLampIndicatorWidget.h"

#include "Character/HollowExoFrameCharacter.h"
#include "Classes/HollowClassDefinition.h"
#include "Components/TextBlock.h"
#include "Foundation/HollowUISettings.h"
#include "InputAction.h"
#include "ViewModels/HollowVisorContext.h"
#include "Visor/HollowVisorStyle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowLampIndicatorWidget)

void UHollowLampIndicatorWidget::BindContext(UHollowVisorContext& InContext)
{
	SourcesHandle = InContext.OnSourcesChanged.AddUObject(this, &ThisClass::Rebind);
	InputTypeHandle = InContext.OnInputTypeChanged.AddWeakLambda(this, [this](ECommonInputType) { Refresh(); });
	KeysHandle = InContext.OnKeysChanged.AddUObject(this, &ThisClass::Refresh);
	ClassHandle = InContext.OnClassChanged.AddUObject(this, &ThisClass::Refresh);   // a lit lamp in the class's colour (S6-01)
	Rebind();
}

void UHollowLampIndicatorWidget::UnbindContext(UHollowVisorContext& InContext)
{
	InContext.OnSourcesChanged.Remove(SourcesHandle);
	InContext.OnInputTypeChanged.Remove(InputTypeHandle);
	InContext.OnKeysChanged.Remove(KeysHandle);
	InContext.OnClassChanged.Remove(ClassHandle);
	if (AHollowExoFrameCharacter* Old = Character.Get())
	{
		Old->OnHelmetLampChanged.Remove(LampHandle);
	}
	Character.Reset();
}

void UHollowLampIndicatorWidget::Rebind()
{
	AHollowExoFrameCharacter* NewCharacter = GetContext() ? GetContext()->GetCharacter() : nullptr;
	if (Character.Get() != NewCharacter)
	{
		if (AHollowExoFrameCharacter* Old = Character.Get())
		{
			Old->OnHelmetLampChanged.Remove(LampHandle);
		}
		Character = NewCharacter;
		if (NewCharacter)
		{
			LampHandle = NewCharacter->OnHelmetLampChanged.AddUObject(this, &ThisClass::HandleLamp);
		}
	}
	Refresh();
}

void UHollowLampIndicatorWidget::HandleLamp(bool bOn)
{
	Refresh();
}

FText UHollowLampIndicatorWidget::GetLampText() const
{
	const UHollowVisorStyle* Style = GetContext() ? GetContext()->GetStyle() : nullptr;
	const AHollowExoFrameCharacter* Body = Character.Get();
	if (!Style || !Body)
	{
		return FText::GetEmpty();
	}
	const FText& State = Body->IsHelmetLampOn() ? Style->LampOnLabel : Style->LampOffLabel;
	const FText Key = GetContext()->GetKeyLabel(GetDefault<UHollowUISettings>()->HelmetLampAction.LoadSynchronous());
	return Key.IsEmpty() ? State : FText::Format(Style->LampFormat, Key, State);
}

void UHollowLampIndicatorWidget::Refresh()
{
	const AHollowExoFrameCharacter* Body = Character.Get();
	SetVisibility(Body && !Body->IsDead() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!Body)
	{
		return;
	}
	const UHollowVisorStyle* Style = GetContext() ? GetContext()->GetStyle() : nullptr;
	LampText->SetText(GetLampText());
	const UHollowClassDefinition* Class = GetContext() ? GetContext()->GetClass() : nullptr;
	const FLinearColor OnColor = Class ? Class->VisorColor : Style ? Style->LampOnColor : FLinearColor::White;
	LampText->SetColorAndOpacity(FSlateColor(Body->IsHelmetLampOn() ? OnColor : Style ? Style->LampOffColor : FLinearColor::White));
}

FString UHollowLampIndicatorWidget::Describe() const
{
	const AHollowExoFrameCharacter* Body = Character.Get();
	return FString::Printf(TEXT("lamp \"%s\" (%s)"), *GetLampText().ToString(), Body ? (Body->IsHelmetLampOn() ? TEXT("on") : TEXT("off")) : TEXT("no body"));
}
