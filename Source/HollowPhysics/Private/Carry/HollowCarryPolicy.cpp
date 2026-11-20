#include "Carry/HollowCarryPolicy.h"

#include "Movement/HollowMovementTuning.h"

namespace HollowCarryPolicy
{
	EHollowCarryClaim Slower(const EHollowCarryClaim A, const EHollowCarryClaim B, const FHollowCarryMovementTuning& Tuning)
	{
		return Tuning.GetSpeedScale(B) < Tuning.GetSpeedScale(A) ? B : A;
	}

	EHollowCarryClaim ServerFloor(const FHollowServerCarry& Server, const double SecondsSinceChange, const float GraceSeconds,
		const FHollowCarryMovementTuning& Tuning)
	{
		const bool bStricter = Tuning.GetSpeedScale(Server.Current) < Tuning.GetSpeedScale(Server.Previous);
		const bool bInGrace = FMath::Max(SecondsSinceChange, 0.0) < static_cast<double>(GraceSeconds);
		return bStricter && bInGrace ? Server.Previous : Server.Current;
	}

	EHollowCarryClaim Effective(const EHollowCarryClaim Claimed, const FHollowServerCarry& Server, const double SecondsSinceChange,
		const float GraceSeconds, const FHollowCarryMovementTuning& Tuning)
	{
		return Slower(Claimed, ServerFloor(Server, SecondsSinceChange, GraceSeconds, Tuning), Tuning);
	}
}
