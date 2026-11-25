#pragma once

#include "CoreMinimal.h"

/** Pure rules of the Whisperer swarms (GP-02, ADR-0047; covered by Hollow.AI.Swarm.Rules). */
namespace HollowSwarmRules
{
	/** How many Whisperers a nest with Seed holds: Min..Max, the same for the same seed. */
	HOLLOWAI_API int32 BroodSize(int32 Seed, int32 Min, int32 Max);

	/** Seed of member Index of a nest with Seed (what it rolls of its own: its drop). */
	HOLLOWAI_API int32 MemberSeed(int32 NestSeed, int32 Index);

	/**
	 * Where member Index of Count comes out round a nest at Center: evenly round a ring of Radius, the ring turned by the
	 * seed (in the plane).
	 */
	HOLLOWAI_API FVector BroodSpot(int32 Seed, int32 Index, int32 Count, const FVector& Center, double Radius);

	/**
	 * Whether a Whisperer may start a bite: while its prey has a light at most MaxBiters bite it at once (Holders: the
	 * others biting it now); in a frenzy every one of them.
	 */
	HOLLOWAI_API bool CanBite(int32 Holders, int32 MaxBiters, bool bFrenzied);

	/** In the dark (no beam on it, not exposed) with prey whose light is off: the frenzy. */
	HOLLOWAI_API bool ShouldFrenzy(bool bLit, bool bExposed, bool bHasPrey, bool bPreyHasLight);

	/** Where it backs away to from a light at LightOrigin: Distance on from From, straight away in the plane. */
	HOLLOWAI_API FVector RecoilGoal(const FVector& From, const FVector& LightOrigin, double Distance);

	/** How dear prey is (the least is hunted): its squared distance, a lit one LitScale times farther. */
	HOLLOWAI_API double PreyCost(double DistanceSquared, bool bPreyHasLight, double LitScale);
}
