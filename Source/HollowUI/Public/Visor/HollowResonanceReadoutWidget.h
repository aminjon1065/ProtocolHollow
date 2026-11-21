#pragma once

#include "Resonance/HollowResonanceTypes.h"
#include "ViewModels/HollowResonanceTierWatch.h"
#include "Visor/HollowVisorElement.h"

#include "HollowResonanceReadoutWidget.generated.h"

class UHollowSquadResonanceComponent;
class UProgressBar;
class UTextBlock;
struct FHollowSquadResonanceNetState;

/**
 * Visor telemetry of the squad's Resonance (S4-08): "RES 63%" and the tier, in the tier's colour from DA_VisorStyle.
 * The level follows the replicated squad state, the tier the tag the player carries (without a body: the squad's).
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowResonanceReadoutWidget : public UHollowVisorElement
{
	GENERATED_BODY()

public:
	int32 GetPercent() const { return Percent; }
	EHollowResonanceTier GetTier() const { return Tier; }
	virtual FString Describe() const override;

protected:
	virtual void BindContext(UHollowVisorContext& InContext) override;
	virtual void UnbindContext(UHollowVisorContext& InContext) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ValueText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TierText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> Bar;

private:
	void Rebind();
	void HandleSquadChanged(const FHollowSquadResonanceNetState& State);
	void Refresh();

	FHollowResonanceTierWatch TierWatch;
	TWeakObjectPtr<UHollowSquadResonanceComponent> Squad;
	FDelegateHandle SourcesHandle;
	FDelegateHandle SquadHandle;
	EHollowResonanceTier Tier = EHollowResonanceTier::Low;
	float Normalized = 0.f;
	int32 Percent = 0;
};
