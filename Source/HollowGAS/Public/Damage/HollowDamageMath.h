#pragma once

#include "CoreMinimal.h"

/** What a hit did, as the shooter's hit marker shows it (GP-01c, ADR-0046). */
enum class EHollowHitKind : uint8
{
	None,
	/** It hurt. */
	Hit,
	/** Armour took most of it, or the target is immune: a dull clank. */
	Armor,
	/** The target is exposed (an Aether lantern's light): a bright ping. */
	Crit
};

HOLLOWGAS_API const TCHAR* LexToString(EHollowHitKind Kind);

/** How Base points of damage met their target (HollowDamageRules::Resolve). */
struct FHollowDamageOutcome
{
	float Damage = 0.f;
	bool bImmune = false;
	bool bExposed = false;
};

/** Pure damage math of UHollowDamageExecution (CLAUDE.md sec. 7.3: covered by Hollow.GAS.Damage.*). */
namespace HollowDamageMath
{
	/**
	 * The hit marker of Outcome: Crit on an exposed target that took damage, Armor if immune or if the armour took at
	 * least ArmorShare of Base, Hit otherwise; None if Base was nothing.
	 */
	HOLLOWGAS_API EHollowHitKind Classify(float Base, const FHollowDamageOutcome& Outcome, float ArmorShare);

	/**
	 * Damage dealt by Base points: nothing if immune or not positive, Base if armor does not apply (or K is 0),
	 * otherwise Base x K / (K + Armor x (1 - Penetration)) with negative armor counted as none (GP-01: a hit ignores
	 * Penetration of the armour, clamped to 0..1).
	 */
	HOLLOWGAS_API float Mitigate(float Base, float Armor, float ArmorConstant, bool bArmorApplies, bool bImmune, float Penetration = 0.f);
}
