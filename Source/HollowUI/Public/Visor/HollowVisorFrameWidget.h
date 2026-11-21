#pragma once

#include "Resonance/HollowResonanceTypes.h"
#include "ViewModels/HollowResonanceTierWatch.h"
#include "Visor/HollowVisorElement.h"

#include "HollowVisorFrameWidget.generated.h"

class UImage;
class UMaterialInstanceDynamic;

/**
 * The helmet's glass over the whole view (S4-08, ADR-0026): M_UI_VisorGlass (vignette, rim, faint grime) on Glass.
 * The squad's Resonance tier the player carries (Hollow.State.Resonance.*) sets the glass's alert glow, its pulse and
 * interference from DA_VisorStyle; the material animates them, so nothing ticks here. Never takes input.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowVisorFrameWidget : public UHollowVisorElement
{
	GENERATED_BODY()

public:
	explicit UHollowVisorFrameWidget(const FObjectInitializer& ObjectInitializer);

	EHollowResonanceTier GetTier() const { return TierWatch.GetTier(); }
	/** The alert glow the glass shows now (0 = calm). */
	float GetAlert() const { return Alert; }
	virtual FString Describe() const override;

protected:
	virtual void BindContext(UHollowVisorContext& InContext) override;
	virtual void UnbindContext(UHollowVisorContext& InContext) override;

	/** Parameters of the glass material (M_UI_VisorGlass). */
	UPROPERTY(EditAnywhere, Category = "Visor")
	FName AlertParameter;

	UPROPERTY(EditAnywhere, Category = "Visor")
	FName AlertColorParameter;

	UPROPERTY(EditAnywhere, Category = "Visor")
	FName PulseParameter;

	UPROPERTY(EditAnywhere, Category = "Visor")
	FName InterferenceParameter;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Glass;

private:
	void Rebind();
	void Apply(EHollowResonanceTier Tier);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GlassMaterial;

	FHollowResonanceTierWatch TierWatch;
	FDelegateHandle SourcesHandle;
	float Alert = 0.f;
};
