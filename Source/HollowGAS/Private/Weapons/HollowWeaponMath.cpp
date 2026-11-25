#include "Weapons/HollowWeaponMath.h"

#include "Math/RandomStream.h"

namespace HollowWeaponMath
{
	void PelletDirections(const FVector& Forward, const int32 Pellets, const float SpreadDegrees, const int32 Seed, TArray<FVector>& OutDirections)
	{
		OutDirections.Reset(FMath::Max(Pellets, 0));
		const FVector Axis = Forward.GetSafeNormal();
		const float HalfAngle = FMath::DegreesToRadians(FMath::Max(SpreadDegrees, 0.f));
		FRandomStream Stream(Seed);
		for (int32 Pellet = 0; Pellet < Pellets; ++Pellet)
		{
			OutDirections.Add(HalfAngle > 0.f ? Stream.VRandCone(Axis, HalfAngle) : Axis);
		}
	}

	float DamageAtDistance(const float Base, const float Distance, const float FalloffStart, const float FalloffEnd, const float MinFraction)
	{
		const float Floor = FMath::Clamp(MinFraction, 0.f, 1.f);
		if (Distance <= FalloffStart)
		{
			return Base;
		}
		if (FalloffEnd <= FalloffStart)
		{
			return Base * Floor;   // no ramp: a step at FalloffStart
		}
		const float Alpha = FMath::Clamp((Distance - FalloffStart) / (FalloffEnd - FalloffStart), 0.f, 1.f);
		return Base * FMath::Lerp(1.f, Floor, Alpha);
	}

	EHollowShotVerdict CheckHit(const FHollowShotCheck& Check)
	{
		if (FVector::Dist(Check.ServerOrigin, Check.ClaimedOrigin) > Check.OriginToleranceCm)
		{
			return EHollowShotVerdict::OriginTooFar;
		}
		const FVector Aim = Check.ServerAim.GetSafeNormal();
		const FVector Direction = Check.ClaimedDirection.GetSafeNormal();
		const float MaxAngle = FMath::DegreesToRadians(Check.SpreadDegrees + Check.AimToleranceDegrees);
		if (FVector::DotProduct(Aim, Direction) < FMath::Cos(MaxAngle))
		{
			return EHollowShotVerdict::OutsideCone;
		}
		if (FVector::Dist(Check.ClaimedOrigin, Check.ImpactPoint) > Check.Range + Check.OriginToleranceCm)
		{
			return EHollowShotVerdict::OutOfRange;
		}
		// The target may have moved this far since the owner saw it.
		const float Rewind = FMath::Clamp(Check.LatencySeconds, 0.f, Check.MaxRewindSeconds);
		const FBox Reach = Check.TargetBounds.ExpandBy(Check.BoundsToleranceCm + Check.TargetVelocity.Size() * Rewind);
		if (!Check.TargetBounds.IsValid || !Reach.IsInsideOrOn(Check.ImpactPoint))
		{
			return EHollowShotVerdict::NotOnTarget;
		}
		return Check.bLineOfSight ? EHollowShotVerdict::Accepted : EHollowShotVerdict::Blocked;
	}

	int32 RoundsToLoad(const int32 Magazine, const int32 MagazineSize, const int32 Carried)
	{
		return FMath::Clamp(MagazineSize - Magazine, 0, FMath::Max(Carried, 0));
	}

	const TCHAR* LexVerdict(const EHollowShotVerdict Verdict)
	{
		switch (Verdict)
		{
		case EHollowShotVerdict::Accepted: return TEXT("Accepted");
		case EHollowShotVerdict::OriginTooFar: return TEXT("OriginTooFar");
		case EHollowShotVerdict::OutsideCone: return TEXT("OutsideCone");
		case EHollowShotVerdict::OutOfRange: return TEXT("OutOfRange");
		case EHollowShotVerdict::NotOnTarget: return TEXT("NotOnTarget");
		case EHollowShotVerdict::Blocked: return TEXT("Blocked");
		}
		return TEXT("?");
	}
}
