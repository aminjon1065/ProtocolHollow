#pragma once

#include "Visor/HollowVisorElement.h"

#include "HollowObjectiveWidget.generated.h"

class UHollowObjectiveModel;
class UTextBlock;

/**
 * The visor's objective tracker (UX-01, ADR-0042): "OBJECTIVE" over what to do now, from the visor context's
 * UHollowObjectiveModel. Hidden while there is nothing to say (front end). Event driven, never ticks.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowObjectiveWidget : public UHollowVisorElement
{
	GENERATED_BODY()

public:
	FText GetObjectiveText() const;
	bool IsShown() const;
	virtual FString Describe() const override;

protected:
	virtual void BindContext(UHollowVisorContext& InContext) override;
	virtual void UnbindContext(UHollowVisorContext& InContext) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ObjectiveText;

private:
	void Refresh();

	TWeakObjectPtr<UHollowObjectiveModel> Objective;
	FDelegateHandle ObjectiveHandle;
	FDelegateHandle ClassHandle;
};
