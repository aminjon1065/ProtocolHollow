#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"

/** A crew log a datapad of the raid's sector may hold. */
struct HOLLOWGAME_API FHollowLoreCandidate
{
	FPrimaryAssetId Id;
	/** Its canon number (UHollowLoreEntryDefinition::Number): the tie-break's key. */
	int32 Number = 0;
};

/** NAR-01b (ADR-0050): which crew log a datapad holds. Pure, tested by Hollow.Game.Lore.Pick. */
namespace HollowLoreRules
{
	/**
	 * The log a datapad of this raid gives the squad: of Candidates (the sector's) not Taken yet this raid, the one the
	 * fewest of the Squad (each player's known logs, from their loadout) have found; between equals the one a hash of
	 * Seed (the sector's) and its number puts first. Deterministic; invalid when every candidate is taken.
	 */
	HOLLOWGAME_API FPrimaryAssetId PickEntry(TConstArrayView<FHollowLoreCandidate> Candidates, TConstArrayView<FPrimaryAssetId> Taken,
		TConstArrayView<TArray<FPrimaryAssetId>> Squad, int32 Seed);

	/** How many of All the Known logs hold (the Codex's "3/12"). */
	HOLLOWGAME_API int32 CountKnown(TConstArrayView<FPrimaryAssetId> All, TConstArrayView<FPrimaryAssetId> Known);
}
