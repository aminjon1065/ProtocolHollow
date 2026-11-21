#include "Carry/HollowCarryCoop.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowCarryCoop)

namespace HollowCarryCoop
{
	FTransform Pose(const FTransform& Primary, const FTransform& Secondary, const double GripHeight)
	{
		const FVector Grip(0.0, 0.0, GripHeight);
		const FVector A = Primary.TransformPosition(Grip);
		const FVector B = Secondary.TransformPosition(Grip);
		FVector Along = B - A;
		Along.Z = 0.0;
		if (!Along.Normalize())
		{
			Along = Primary.GetRotation().GetForwardVector().GetSafeNormal2D();
		}
		return FTransform(FRotationMatrix::MakeFromX(Along).ToQuat(), (A + B) * 0.5);
	}
}

namespace HollowCarryTether
{
	FVector Constrain(const FVector& Velocity, const FVector& Self, const FVector& Partner, const FHollowCoopCarryTuning& Tuning, const float MaxSpeed)
	{
		FVector Ground(Velocity.X, Velocity.Y, 0.0);
		FVector ToPartner = Partner - Self;
		ToPartner.Z = 0.0;
		const double Distance = ToPartner.Size();
		if (Distance > UE_KINDA_SMALL_NUMBER)
		{
			const FVector Direction = ToPartner / Distance;
			const double Towards = FVector::DotProduct(Ground, Direction);
			const double Stretch = Distance - (Tuning.CarrierSpacing + Tuning.Slack);
			if (Stretch > 0.0)
			{
				const double Pull = FMath::Min(Tuning.PullRate * Stretch, static_cast<double>(Tuning.MaxPullSpeed));
				Ground += Direction * FMath::Max(Pull - Towards, 0.0);
			}
			else if (Distance < Tuning.CarrierSpacing - Tuning.Slack && Towards > 0.0)
			{
				Ground -= Direction * Towards;
			}
		}
		Ground = Ground.GetClampedToMaxSize(FMath::Max(MaxSpeed, 0.f));
		return FVector(Ground.X, Ground.Y, Velocity.Z);
	}

	bool IsBroken(const FVector& A, const FVector& B, const FHollowCoopCarryTuning& Tuning)
	{
		return FVector::DistSquared(A, B) > FMath::Square(static_cast<double>(Tuning.GetMaxSeparation()));
	}
}

namespace HollowCoopSanity
{
	bool IsTooFast(const FVector& From, const FVector& To, const double Seconds, const float MaxSpeed, const float Tolerance)
	{
		const double Limit = static_cast<double>(MaxSpeed) * Tolerance * FMath::Max(Seconds, 0.0);
		return FVector::DistSquared2D(From, To) > FMath::Square(Limit);
	}

	double MovedFor(const double ServerSeconds, const double ClientSeconds)
	{
		return FMath::Max(ServerSeconds, ClientSeconds > 0.0 ? ClientSeconds : 0.0);
	}

	bool DoesSecondaryLetGo(const double PrimaryMoved, const double SecondaryMoved)
	{
		return SecondaryMoved >= PrimaryMoved;
	}
}

bool FHollowCoopReturn::ShouldReturn(const double Now, const bool bInSync, const float SettleSeconds, const float TimeoutSeconds) const
{
	if (Now - LeftAt >= TimeoutSeconds)
	{
		return true;
	}
	return KnownAt.IsSet() && Now - KnownAt.GetValue() >= SettleSeconds && bInSync;
}
