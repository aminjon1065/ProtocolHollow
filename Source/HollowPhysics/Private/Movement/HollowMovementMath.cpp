#include "Movement/HollowMovementMath.h"

#include "Movement/HollowMovementTuning.h"

namespace HollowMovementMath
{
	bool CanSprint(const FHollowGroundSpeedTuning& Tuning, const bool bWantsToSprint, const bool bIsCrouching,
		const FVector& MoveDirection, const FVector& FacingDirection)
	{
		if (!bWantsToSprint || bIsCrouching || Tuning.SprintSpeedMultiplier <= 1.f)
		{
			return false;
		}

		const FVector Move2D = MoveDirection.GetSafeNormal2D();
		const FVector Facing2D = FacingDirection.GetSafeNormal2D();
		if (Move2D.IsNearlyZero() || Facing2D.IsNearlyZero())
		{
			return false;
		}

		const float MinCosine = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(Tuning.SprintMaxAngle, 0.f, 90.f)));
		return FVector::DotProduct(Move2D, Facing2D) >= MinCosine - UE_KINDA_SMALL_NUMBER;
	}

	float GetSpeedMultiplier(const FHollowGroundSpeedTuning& Tuning, const bool bWantsToSprint, const bool bIsCrouching,
		const FVector& MoveDirection, const FVector& FacingDirection)
	{
		return CanSprint(Tuning, bWantsToSprint, bIsCrouching, MoveDirection, FacingDirection) ? Tuning.SprintSpeedMultiplier : 1.f;
	}
}
