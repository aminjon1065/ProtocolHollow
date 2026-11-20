#pragma once

#include "Foundation/HollowActivatableWidget.h"
#include "Profile/HollowProfileTypes.h"

#include "HollowDebriefWidget.generated.h"

class UCommonButtonBase;
class UTextBlock;

/**
 * Minimal debrief (ADR-0016) on the Menu layer: how the raid ended, what the squad brought out, credits before and
 * after, what was kept and lost. Shown when a raid is resolved (the host's result, a lost host, an abandoned raid
 * found at startup) and again on the next screen until the player closes it.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowDebriefWidget : public UHollowActivatableWidget
{
	GENERATED_BODY()

public:
	explicit UHollowDebriefWidget(const FObjectInitializer& ObjectInitializer);

	void SetReport(const FHollowRaidReport& InReport);

	UCommonButtonBase* GetCloseButton() const { return CloseButton; }
	FText GetTitle() const;
	FText GetDetails() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DetailsText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> CloseButton;

private:
	void Close();
	static FText OutcomeTitle(EHollowRaidOutcome Outcome);
	static FText DescribeItems(const TArray<FHollowItemStack>& Stacks);

	FHollowRaidReport Report;
};
