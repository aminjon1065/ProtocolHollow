#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

class AHollowEnemyCharacter;
class UWorld;

namespace HollowEnemySpawning
{
	/**
	 * Server: spawns an enemy of Class standing on its own navmesh (the agent of its capsule) at the point nearest to
	 * Wanted within SearchExtent; null if there is no navmesh there.
	 */
	HOLLOWAI_API AHollowEnemyCharacter* SpawnOnNavmesh(UWorld* World, TSubclassOf<AHollowEnemyCharacter> Class, const FVector& Wanted,
		float SearchExtent, const FRotator& Facing);
}
