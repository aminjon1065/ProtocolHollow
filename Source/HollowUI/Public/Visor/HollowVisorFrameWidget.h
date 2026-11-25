#pragma once

#include "Resonance/HollowResonanceTypes.h"
#include "ViewModels/HollowAttributeWatch.h"
#include "ViewModels/HollowResonanceTierWatch.h"
#include "Visor/HollowVisorElement.h"

#include "HollowVisorFrameWidget.generated.h"

class UImage;
class UMaterialInstanceDynamic;

/**
 * The helmet's glass over the whole view (S4-08, ADR-0026): M_UI_VisorGlass (vignette, rim, faint grime) on Glass.
 * The squad's Resonance tier the player carries (Hollow.State.Resonance.*) sets the glass's alert glow, its pulse and
 * interference from DA_VisorStyle; the material animates them, so nothing ticks here. Never takes input.
 * With the player's reduced flashes (ADR-0029) the glow holds steady instead of pulsing.
 * GP-01c (ADR-0046): the edges flash in DA_VisorStyle's damage colour as the player loses health, stronger the more it
 * lost, fading on a short timer; with reduced flashes a dim edge fades slowly instead.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowVisorFrameWidget : public UHollowVisorElement
{
	GENERATED_BODY()

public:
	explicit UHollowVisorFrameWidget(const FObjectInitializer& ObjectInitializer);

	EHollowResonanceTier GetTier() const { return TierWatch.GetTier(); }
	/** The alert glow the glass shows now (0 = calm), and its pulse (0 = steady). */
	float GetAlert() const { return Alert; }
	float GetPulseHz() const { return PulseHz; }
	/** The damage edge shown now (0 = none) and whether it is still fading. */
	float GetDamageLevel() const { return DamageLevel; }
	bool IsDamageFading() const;
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

	UPROPERTY(EditAnywhere, Category = "Visor")
	FName DamageParameter;

	UPROPERTY(EditAnywhere, Category = "Visor")
	FName DamageColorParameter;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Glass;

private:
	void Rebind();
	void Apply(EHollowResonanceTier Tier);
	void HandleReducedFlashesChanged();
	void HandleHealth(float Value, float Max);
	/** The edge flashes for a loss of Share of the health. */
	void FlashDamage(float Share);
	void StepDamage();
	void ShowDamage(float Level);
	bool EnsureMaterial();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GlassMaterial;

	FHollowResonanceTierWatch TierWatch;
	FHollowAttributeWatch HealthWatch;
	FTimerHandle DamageTimer;
	float LastHealth = -1.f;
	float DamagePeak = 0.f;
	float DamageLevel = 0.f;
	float DamageSeconds = 0.f;
	double DamageStart = 0.0;
	bool bDamageReduced = false;
	FDelegateHandle SourcesHandle;
	FDelegateHandle ReducedFlashesHandle;
	float Alert = 0.f;
	float PulseHz = 0.f;
};
