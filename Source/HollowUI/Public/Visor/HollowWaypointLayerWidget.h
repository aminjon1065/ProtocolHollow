#pragma once

#include "Visor/HollowObjectiveRules.h"
#include "Visor/HollowVisorElement.h"

#include "HollowWaypointLayerWidget.generated.h"

class UCanvasPanel;
class UHollowObjectiveModel;
class UHollowWaypointMarkerWidget;

/**
 * The visor's waypoints (UX-01, ADR-0042): the places the objective marks, each with its distance, on the place while
 * it is in view and at the edge with an arrow otherwise, so a player never loses the way in the generated sector.
 * A timer at UHollowVisorStyle::WaypointRateHz places them while any is to be shown; no Tick. Under a hub screen
 * they step aside.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowWaypointLayerWidget : public UHollowVisorElement
{
	GENERATED_BODY()

public:
	/** One placed marker (tests, Hollow.UI.Dump). */
	struct FShown
	{
		EHollowWaypointKind Kind = EHollowWaypointKind::Extraction;
		int32 Metres = 0;
		FHollowWaypointPlacement Placement;
	};

	/** Places the markers now (the timer does it while any is to be shown). */
	void UpdateMarkers();

	/** Tests: the visor's size without a viewport. */
	void SetViewSizeForTests(const FVector2D& Size) { TestViewSize = Size; }

	/** Tests: the readouts to keep off, in the visor's space (none laid out without a viewport). */
	void SetKeepOutForTests(const TArray<FBox2D>& Boxes) { TestKeepOut = Boxes; }

	/** The visor's readouts the markers keep off now (UHollowVisorStyle::WaypointKeepOut, as laid out). */
	TArray<FBox2D> CollectKeepOut() const;

	const TArray<FShown>& GetShown() const { return Shown; }
	bool IsUpdating() const;

	virtual FString Describe() const override;

protected:
	virtual void BindContext(UHollowVisorContext& InContext) override;
	virtual void UnbindContext(UHollowVisorContext& InContext) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> MarkerCanvas;

	/** W_VisorWaypoint (create_visor_assets.py). */
	UPROPERTY(EditAnywhere, Category = "Waypoints")
	TSubclassOf<UHollowWaypointMarkerWidget> MarkerClass;

private:
	/** Starts or stops the timer for what the objective marks now. */
	void Restart();
	void HideFrom(int32 FirstHidden);
	FVector2D GetViewSize() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHollowWaypointMarkerWidget>> Markers;

	TArray<FShown> Shown;
	TWeakObjectPtr<UHollowObjectiveModel> Objective;
	FDelegateHandle ObjectiveHandle;
	FTimerHandle UpdateTimer;
	FVector2D TestViewSize = FVector2D::ZeroVector;
	TArray<FBox2D> TestKeepOut;
};
