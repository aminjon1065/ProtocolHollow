#pragma once

#include "CommonButtonBase.h"

#include "HollowButton.generated.h"

class UTextBlock;

/** CommonUI button with a label set per instance (menus); look comes from its CommonButtonStyle (data). */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowButton : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	const FText& GetLabel() const { return Label; }

	/** Changes the label while the menu is up (a toggle shows its state, S4-07). */
	void SetLabel(const FText& InLabel);

protected:
	virtual void NativePreConstruct() override;

	UPROPERTY(EditAnywhere, Category = "Button")
	FText Label;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelText;
};
