#pragma once

#include "CommonButtonBase.h"

#include "HollowButton.generated.h"

class UTextBlock;

/**
 * CommonUI button with a label set per instance (menus); look and sounds come from its CommonButtonStyle (data). UI-02
 * (ADR-0052): a refused button stays focusable and pressable, greyed out; its press plays the style's deny sound and
 * tells OnLockClicked (its screen shows why) instead of OnClicked. A focus moved onto it by the keys plays the focus
 * sound the mouse plays on hover, and while the keys or a gamepad hold the focus, FocusBrush frames it (the style's
 * hover look follows the mouse only).
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowButton : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	explicit UHollowButton(const FObjectInitializer& ObjectInitializer);

	const FText& GetLabel() const { return Label; }

	/** Changes the label while the menu is up (a toggle shows its state, S4-07). */
	void SetLabel(const FText& InLabel);

	/** Refuses (or allows again) its press: CommonUI's locked state, greyed out by DA_UIStyle::RefusedOpacity. */
	void SetRefused(bool bRefused);
	bool IsRefused() const { return GetLocked(); }

	/** Whether the focus frame shows now (keys or a gamepad focus it). */
	bool ShowsFocus() const { return bShowsFocus; }

	/** Drawn over the button while the keys or a gamepad focus it (a rounded frame: hollow_ui_style.py). */
	UPROPERTY(EditAnywhere, Category = "Button")
	FSlateBrush FocusBrush;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	UPROPERTY(EditAnywhere, Category = "Button")
	FText Label;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelText;

private:
	bool bShowsFocus = false;
};
