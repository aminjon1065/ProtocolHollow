#pragma once

#include "Visor/HollowVisorElement.h"

#include "HollowTargetStatusWidget.generated.h"

class UHorizontalBox;
class UProgressBar;
class UTextBlock;

/**
 * Bar of the active tool's target (UHollowToolTargetModel): "Welding 63%" with the target's threshold marked.
 * MarkerRow (optional) is spacer | marker | spacer; the spacers' fill splits the row at the marker.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowTargetStatusWidget : public UHollowVisorElement
{
	GENERATED_BODY()

public:
	explicit UHollowTargetStatusWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BindContext(UHollowVisorContext& InContext) override;
	virtual void UnbindContext(UHollowVisorContext& InContext) override;

	/** {0} = the tool's action label, {1} = the value in percent. */
	UPROPERTY(EditAnywhere, Category = "Visor")
	FText LabelFormat;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> Bar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> MarkerRow;

private:
	void Refresh();

	FDelegateHandle TargetHandle;
};
