#pragma once

#include "GameplayCueNotify_Static.h"

#include "HollowWeaponFireCue.generated.h"

/**
 * One weapon shot (GameplayCue.Hollow.Weapon.Fire, GP-01), purely cosmetic and local to every machine: the report (the
 * weapon's noise tag in the sound bank), the muzzle flash at the held weapon's muzzle, a tracer for every pellet and a
 * puff where it lands. The pellets are traced here again from the cue's origin, aim and serial (the owner's exact shot;
 * everyone else's from the server's view of the shooter), so no hit point is replicated. The weapon's systems are
 * data (UHollowWeaponDefinition), pooled by Niagara; their +Z points along the shot (tracers, flash) or the surface
 * (impacts). Never ticks.
 */
UCLASS()
class HOLLOWGAME_API UHollowWeaponFireCue : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
};
