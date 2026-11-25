#pragma once

#include "Damage/HollowDamageMath.h"
#include "Templates/SubclassOf.h"

class AActor;
class UGameplayEffect;

/** The shooter's own hit marker (GP-01c, ADR-0046): what it would show for a hit, from what its machine knows. */
namespace HollowHitMarker
{
	/**
	 * Target hit for Damage by DamageEffect: a squad mate shows nothing (no friendly fire), a target with health its
	 * HollowDamageRules::Preview, a breakable prop (a crate) a plain hit, anything else nothing.
	 */
	HOLLOWGAME_API EHollowHitKind KindOf(const AActor* Target, TSubclassOf<UGameplayEffect> DamageEffect, float Damage, float Penetration,
		float ExposedPenetration);

	/** The one to show of two: a crit over a hit over armour. */
	HOLLOWGAME_API EHollowHitKind Strongest(EHollowHitKind A, EHollowHitKind B);
}
