#pragma once

#include "Data/HollowToolDefinition.h"

#include "HollowLanternDefinition.generated.h"

/**
 * The Aether lantern (S6-02, ADR-0035): a continuous tool whose samples light a cone instead of working on a target.
 * Fuel, heat, overheat, noise and sampling are the tool's (UHollowToolDefinition); Range is the cone's reach and
 * TraceChannel its line of sight. Enemies in the cone and in sight get ExposeEffect, living players within AuraRadius
 * of the wielder (the wielder too) get AuraEffect; both are renewed every sample. BeamColor tints the light.
 */
UCLASS(BlueprintType)
class HOLLOWGAS_API UHollowLanternDefinition : public UHollowToolDefinition
{
	GENERATED_BODY()

public:
	/** Half angle of the light cone around the wielder's view. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lantern", meta = (ClampMin = "1", ClampMax = "89", Units = "Degrees"))
	float ConeHalfAngleDegrees = 0.f;

	/** On every enemy in the light (GE_Occult_Expose, UHollowOccultExposeEffect): its duration is how long it lingers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lantern")
	TSubclassOf<UGameplayEffect> ExposeEffect;

	/** Radius around the wielder whose living players get AuraEffect (no line of sight needed). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lantern", meta = (ClampMin = "0", Units = "Centimeters"))
	float AuraRadius = 0.f;

	/** On every player within the aura (GE_AntiResonance, UHollowAntiResonanceEffect). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lantern")
	TSubclassOf<UGameplayEffect> AuraEffect;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
