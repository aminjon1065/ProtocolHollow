#pragma once

#include "CoreMinimal.h"

/** Pure damage math of UHollowDamageExecution (CLAUDE.md sec. 7.3: covered by Hollow.GAS.Damage.*). */
namespace HollowDamageMath
{
	/**
	 * Damage dealt by Base points: nothing if immune or not positive, Base if armor does not apply (or K is 0),
	 * otherwise Base x K / (K + Armor) with negative armor counted as none.
	 */
	HOLLOWGAS_API float Mitigate(float Base, float Armor, float ArmorConstant, bool bArmorApplies, bool bImmune);
}
