#include "Threat/HollowSwarmRules.h"

#include "Math/RandomStream.h"

namespace HollowSwarmRulesPrivate
{
	/** SplitMix64 finalizer (Steele et al. 2014): neighbouring inputs give unrelated outputs (FRandomStream's first draw does not). */
	constexpr uint64 Mix(uint64 Value)
	{
		Value += 0x9E3779B97F4A7C15ULL;
		Value = (Value ^ (Value >> 30u)) * 0xBF58476D1CE4E5B9ULL;
		Value = (Value ^ (Value >> 27u)) * 0x94D049BB133111EBULL;
		return Value ^ (Value >> 31u);
	}
}

int32 HollowSwarmRules::BroodSize(const int32 Seed, const int32 Min, const int32 Max)
{
	if (Max <= Min)
	{
		return FMath::Max(Min, 0);
	}
	return FMath::Max(FRandomStream(Seed).RandRange(Min, Max), 0);
}

int32 HollowSwarmRules::MemberSeed(const int32 NestSeed, const int32 Index)
{
	// Well mixed: the members of one brood roll their drops apart, not all alike.
	const uint64 Key = (static_cast<uint64>(static_cast<uint32>(NestSeed)) << 32u) | static_cast<uint64>(static_cast<uint32>(Index + 1));
	return static_cast<int32>(HollowSwarmRulesPrivate::Mix(Key) & 0x7FFFFFFFULL);
}

FVector HollowSwarmRules::BroodSpot(const int32 Seed, const int32 Index, const int32 Count, const FVector& Center, const double Radius)
{
	if (Count <= 0)
	{
		return Center;
	}
	// The stream's first draw turns the ring; the second (the size) is BroodSize's: a different stream keeps them apart.
	const double Turn = FRandomStream(MemberSeed(Seed, -1)).FRandRange(0.f, 2.f * UE_PI);
	const double Angle = Turn + 2.0 * UE_DOUBLE_PI * Index / Count;
	return Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0) * Radius;
}

bool HollowSwarmRules::CanBite(const int32 Holders, const int32 MaxBiters, const bool bFrenzied)
{
	return bFrenzied || Holders < MaxBiters;
}

bool HollowSwarmRules::ShouldFrenzy(const bool bLit, const bool bExposed, const bool bHasPrey, const bool bPreyHasLight)
{
	return !bLit && !bExposed && bHasPrey && !bPreyHasLight;
}

FVector HollowSwarmRules::RecoilGoal(const FVector& From, const FVector& LightOrigin, const double Distance)
{
	FVector Away = (From - LightOrigin).GetSafeNormal2D();
	if (Away.IsNearlyZero())
	{
		Away = FVector::ForwardVector;   // the light is right on top of it: any way out
	}
	return From + Away * Distance;
}

double HollowSwarmRules::PreyCost(const double DistanceSquared, const bool bPreyHasLight, const double LitScale)
{
	return bPreyHasLight ? DistanceSquared * LitScale * LitScale : DistanceSquared;
}
