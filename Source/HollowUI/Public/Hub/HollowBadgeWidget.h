#pragma once

#include "CommonUserWidget.h"
#include "Hub/HollowHubBadge.h"

#include "HollowBadgeWidget.generated.h"

class UBorder;
class UPanelWidget;
class UTextBlock;

/** A coloured badge of a hub screen (W_HubBadge, UI-02, ADR-0052): a framed glass chip with a few words. */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowBadgeWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** Shows Badge in its kind's colours (DA_UIStyle). */
	void SetBadge(const FHollowHubBadge& Badge);

	EHollowHubBadgeKind GetKind() const { return Kind; }
	FText GetText() const;

	/** Shows Badges in Box, reusing its badge widgets (none: the box collapses). */
	static void FillBox(UPanelWidget* Box, TConstArrayView<FHollowHubBadge> Badges, UUserWidget& Owner);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> Frame;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Label;

private:
	EHollowHubBadgeKind Kind = EHollowHubBadgeKind::Price;
};
