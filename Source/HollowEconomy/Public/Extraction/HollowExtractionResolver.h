#pragma once

#include "Extraction/HollowRaidTypes.h"

/**
 * Turns one player's raid into the change to their profile (Docs/Architecture/02 sec. 2.5, ADR-0005, ADR-0016).
 * Pure, runs on the player's own machine. Tested by Hollow.Economy.Resolver.*:
 * - Extracted: the whole loadout is kept and the loot is sold (each extracted player gets the full squad loot).
 * - Dead, MissingInAction, Abandoned: only insured slots of insurable items are kept, the loot is lost.
 * - SignalLost: the loadout is kept, the loot is lost.
 * Credits already in the profile are never touched; insurance is not consumed.
 */
struct HOLLOWECONOMY_API FHollowExtractionResolver
{
	static FHollowProfileDelta Resolve(const FHollowRaidContext& Context, EHollowRaidOutcome Outcome, const FHollowItemCatalog& Catalog);

	/** Does the outcome keep the whole loadout? */
	static bool KeepsLoadout(EHollowRaidOutcome Outcome);

	/** Does the outcome bring the loot home? */
	static bool KeepsLoot(EHollowRaidOutcome Outcome);
};
