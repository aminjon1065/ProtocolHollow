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

protected:
	virtual void NativePreConstruct() override;

	UPROPERTY(EditAnywhere, Category = "Button")
	FText Label;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelText;
};
