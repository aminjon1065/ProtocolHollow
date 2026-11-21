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

	int32 MoveSteps(const float DeltaTime, const float MaxMoveDeltaTime, const int32 MaxMoves)
	{
		if (MaxMoveDeltaTime <= 0.f || DeltaTime <= MaxMoveDeltaTime)
		{
			return 1;
		}
		return FMath::Clamp(FMath::CeilToInt32(DeltaTime / MaxMoveDeltaTime), 1, FMath::Max(MaxMoves, 1));
	}
}
