#pragma once

#include "GameplayCueNotify_Actor.h"

#include "HollowEnemyWindupCue.generated.h"

class UPointLightComponent;

/**
 * Wind-up telegraph of an enemy attack (GameplayCue.Hollow.Enemy.Windup): a glow on the attacker while the blow is
 * coming, on every machine. Greybox: Niagara and sound replace the light on the art pass (GCN_Enemy_Windup is data).
 */
UCLASS(Abstract)
class HOLLOWAI_API AHollowEnemyWindupCue : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	explicit AHollowEnemyWindupCue(const FObjectInitializer& ObjectInitializer);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Cue")
	TObjectPtr<UPointLightComponent> Glow;
};
