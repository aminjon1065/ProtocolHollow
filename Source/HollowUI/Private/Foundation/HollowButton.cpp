#include "Foundation/HollowButton.h"

#include "CommonButtonBase.h"
#include "Components/TextBlock.h"
#include "Foundation/HollowUIStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Sound/SlateSound.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowButton)

UHollowButton::UHollowButton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FocusBrush.DrawAs = ESlateBrushDrawType::NoDrawType;   // no frame until the data gives one
}

void UHollowButton::SetLabel(const FText& InLabel)
{
	Label = InLabel;
	if (LabelText)
	{
		LabelText->SetText(Label);
	}
}

void UHollowButton::SetRefused(const bool bRefused)
{
	SetIsLocked(bRefused);
	const UHollowUIStyle* Style = UHollowUIStyle::Get();
	SetRenderOpacity(bRefused && Style ? Style->RefusedOpacity : 1.f);
}

void UHollowButton::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	bShowsFocus = InFocusEvent.GetCause() != EFocusCause::Mouse;   // a click's focus: the mouse sees its hover already
	Invalidate(EInvalidateWidgetReason::Paint);
	// The mouse hears the style's hover sound from Slate; keys and the gamepad move the focus instead (not on a
	// screen's first focus, which comes with its own sound).
	const UCommonButtonStyle* Style = InFocusEvent.GetCause() == EFocusCause::Navigation ? GetStyleCDO() : nullptr;
	if (Style && Style->HoveredSlateSound.GetResourceObject() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().PlaySound(Style->HoveredSlateSound);
	}
}

void UHollowButton::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnRemovedFromFocusPath(InFocusEvent);
	bShowsFocus = false;
	Invalidate(EInvalidateWidgetReason::Paint);
}

int32 UHollowButton::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, const int32 LayerId, const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const int32 Top = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (!bShowsFocus || FocusBrush.GetDrawType() == ESlateBrushDrawType::NoDrawType)
	{
		return Top;
	}
	FSlateDrawElement::MakeBox(OutDrawElements, Top + 1, AllottedGeometry.ToPaintGeometry(), &FocusBrush, ESlateDrawEffect::None,
		FocusBrush.GetTint(InWidgetStyle) * InWidgetStyle.GetColorAndOpacityTint());
	return Top + 1;
}

void UHollowButton::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (LabelText)
	{
		LabelText->SetText(Label);
	}
}
