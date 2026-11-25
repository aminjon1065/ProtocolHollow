#pragma once

#include "UObject/Interface.h"

#include "HollowSeededActor.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UHollowSeededActor : public UInterface
{
	GENERATED_BODY()
};

/**
 * A gameplay actor the sector spawns at a room socket that rolls something of its own (GP-01b: a loot crate's contents).
 * The server gives it a seed derived from the sector's seed and its socket before it finishes spawning, so the same
 * sector always holds the same (CLAUDE.md sec. 5: determinism from the seed).
 */
class HOLLOWCORE_API IHollowSeededActor
{
	GENERATED_BODY()

public:
	/** Server, before FinishSpawning. */
	virtual void SetSpawnSeed(int32 Seed) = 0;
};
