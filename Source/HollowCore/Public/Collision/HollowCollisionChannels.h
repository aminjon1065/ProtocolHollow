#pragma once

#include "Engine/EngineTypes.h"

/** Project collision channels, declared in DefaultEngine.ini [/Script/Engine.CollisionProfile]. */
namespace HollowCollision
{
	/** Trace channel "Interaction" (default Block): interactables and walls block it, cosmetic debris ignores it. */
	inline constexpr ECollisionChannel Interaction = ECC_GameTraceChannel1;

	/**
	 * Object channel "HollowDebris" (default Block, so walls and floors stop debris): cosmetic Chaos fragments and
	 * rubble (ADR-0002). Their profile ignores pawns, physics bodies and every trace, so debris never moves a player,
	 * an enemy or a carried core and never stops a tool, a line of sight or an interaction.
	 */
	inline constexpr ECollisionChannel Debris = ECC_GameTraceChannel2;

	/** Collision profile of everything on the Debris channel. */
	inline FName DebrisProfile() { return FName(TEXT("HollowDebris")); }

	/** Profile of a free heavy carryable (Aether core): a physics body that blocks the world and pawns, focusable. */
	inline FName CarryableProfile() { return FName(TEXT("HollowCarryable")); }

	/**
	 * Profile of a carryable while carried (ADR-0015): query only and ignores everything except overlaps with world
	 * dynamic volumes (extraction zone), so it never blocks its carrier, the world, traces or interaction.
	 */
	inline FName CarriedProfile() { return FName(TEXT("HollowCarried")); }
}
