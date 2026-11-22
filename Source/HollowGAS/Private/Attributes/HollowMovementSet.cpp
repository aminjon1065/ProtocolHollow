#include "Attributes/HollowMovementSet.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowMovementSet)

namespace HollowMovementSetPrivate
{
	/** A multiplier that changes nothing: what an enemy moves at while no effect slows it. */
	constexpr float NeutralMultiplier = 1.f;
}

UHollowMovementSet::UHollowMovementSet()
{
	InitMoveSpeedMultiplier(HollowMovementSetPrivate::NeutralMultiplier);
}
