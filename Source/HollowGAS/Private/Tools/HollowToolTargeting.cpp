#include "Tools/HollowToolTargeting.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowToolTargeting)

EHollowToolAimResult HollowToolTargeting::Resolve(const FHollowToolAim& Aim, const float MaxAngleDegrees, const float GraceSeconds)
{
	if (Aim.bHitTarget)
	{
		return EHollowToolAimResult::WorkOnHit;
	}
	if (Aim.bHasCurrent && Aim.AngleToCurrentDegrees <= MaxAngleDegrees && Aim.SecondsSinceCurrentHit <= GraceSeconds)
	{
		return EHollowToolAimResult::KeepCurrent;
	}
	return EHollowToolAimResult::Lose;
}
