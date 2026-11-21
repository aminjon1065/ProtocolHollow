#pragma once

#include "GameplayCueNotify_Actor.h"

#include "HollowEnemyWindupCue.generated.h"

class UPointLightComponent;

/**
 * Wind-up telegraph of an enemy attack (GameplayCue.Hollow.Enemy.Windup): a glow on the attacker while the blow is
 * coming, its voice (UHollowEnemyDefinition::WindupSound, S4-04) and the attack's montage (the cue carries the attack
 * as its source object, ADR-0027), on every machine. Greybox: Niagara replaces the
 * light on the art pass (GCN_Enemy_Windup is data).
 */
UCLASS(Abstract)
class HOLLOWAI_API AHollowEnemyWindupCue : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	explicit AHollowEnemyWindupCue(const FObjectInitializer& ObjectInitializer);

	/** The attacker's WindupSound (its definition) on top of the glow. */
	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Cue")
	TObjectPtr<UPointLightComponent> Glow;
};
