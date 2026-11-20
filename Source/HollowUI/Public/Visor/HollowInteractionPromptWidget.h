#pragma once

#include "Visor/HollowVisorElement.h"

#include "HollowInteractionPromptWidget.generated.h"

class UTextBlock;

/** "[E] Open": the focused interactable's prompt with the Interact key of the current device. */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowInteractionPromptWidget : public UHollowVisorElement
{
	GENERATED_BODY()

public:
	explicit UHollowInteractionPromptWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BindContext(UHollowVisorContext& InContext) override;
	virtual void UnbindContext(UHollowVisorContext& InContext) override;

	/** {0} = key label. */
	UPROPERTY(EditAnywhere, Category = "Visor")
	FText KeyFormat;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> KeyText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PromptText;

private:
	void Refresh();

	FDelegateHandle PromptHandle;
};
