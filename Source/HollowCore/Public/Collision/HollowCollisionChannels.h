#pragma once

#include "Engine/EngineTypes.h"

/** Project collision channels, declared in DefaultEngine.ini [/Script/Engine.CollisionProfile]. */
namespace HollowCollision
{
	/** Trace channel "Interaction" (default Block): interactables and walls block it, cosmetic debris ignores it. */
	inline constexpr ECollisionChannel Interaction = ECC_GameTraceChannel1;
}
