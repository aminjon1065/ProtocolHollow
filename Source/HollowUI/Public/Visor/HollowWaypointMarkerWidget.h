#pragma once

#include "CommonUserWidget.h"

#include "HollowWaypointMarkerWidget.generated.h"

class UImage;
class UTextBlock;
struct FHollowWaypointPlacement;

/**
 * One waypoint on the visor (UX-01, ADR-0042; W_VisorWaypoint): a pip on the place while it is in view, an arrow at
 * the visor's edge toward it otherwise, and "EVAC 42 m" under it. Placed by UHollowWaypointLayerWidget; never ticks.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowWaypointMarkerWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void Show(const FText& Label, const FLinearColor& Color, const FHollowWaypointPlacement& Placement);

	FText GetLabel() const;
	bool IsArrowShown() const;

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> LabelText;

	/** On the place itself. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Pip;

	/** At the edge, pointing up until turned toward the place. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Arrow;
};
