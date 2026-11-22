#pragma once

#include "GameplayCueNotify_Actor.h"

#include "HollowExposedCue.generated.h"

class UPointLightComponent;

/**
 * Glow on an enemy in an Aether lantern's light (GameplayCue.Hollow.Status.Exposed, from GE_Occult_Expose, S6-02):
 * a small shadowless light riding on it while the effect lasts, on every machine (an enemy's ability system replicates
 * its cues in Minimal mode). Greybox; the art pass may swap it for an overlay material (GCN_Status_Exposed is data).
 */
UCLASS(Abstract)
class HOLLOWGAME_API AHollowExposedCue : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	explicit AHollowExposedCue(const FObjectInitializer& ObjectInitializer);

	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Exposed")
	TObjectPtr<UPointLightComponent> Glow;

	/** Where the glow sits on the enemy, from its root. */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Exposed")
	FVector GlowOffset = FVector::ZeroVector;
};
