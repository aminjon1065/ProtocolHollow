#pragma once

#include "Foundation/HollowButton.h"
#include "Menus/HollowSettingsLayout.h"

#include "HollowSettingRowWidget.generated.h"

class UTextBlock;

/**
 * One row of the settings screen (W_SettingRow, UI-02, ADR-0052): "Name  < value >", one focusable button. Left / right
 * (D-pad, stick, arrow keys; held: repeated) step its value down / up, so do a click on the arrows and the mouse wheel;
 * a press elsewhere (A, Enter, click) steps it up. Its screen applies the step (OnStep) and shows the new value.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowSettingRowWidget : public UHollowButton
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSettingStep, UHollowSettingRowWidget* /*Row*/, int32 /*Direction*/);

	/** A step asked by the player: +1 up / next, -1 down / previous. */
	FOnSettingStep OnStep;

	void SetSetting(EHollowSettingId InSetting, const FText& InLabel);
	EHollowSettingId GetSetting() const { return Setting; }

	void SetValue(const FText& Value);
	FText GetValue() const;

	/** Asks for a step as the player's input does (tests). */
	void Step(int32 Direction);

protected:
	virtual void NativeOnInitialized() override;
	virtual FNavigationReply NativeOnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent, const FNavigationReply& InDefaultReply) override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ValueText;

	/** The arrows either side of the value (their glyphs are the code's, no player text): a click on one steps that way. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DecreaseHint;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> IncreaseHint;

private:
	void HandleClicked();

	EHollowSettingId Setting = EHollowSettingId::GraphicsPreset;

	/** The way the next click steps (the arrow the mouse went down on); a press without the mouse steps up. */
	int32 ClickDirection = 1;
};
