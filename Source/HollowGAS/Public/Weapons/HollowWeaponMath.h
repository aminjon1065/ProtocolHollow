#pragma once

#include "CoreMinimal.h"

/** Why the server took or refused one of the owner's hits (ADR-0044). */
enum class EHollowShotVerdict : uint8
{
	Accepted,
	/** The owner's eyes were not where the server sees them. */
	OriginTooFar,
	/** The pellet left outside the weapon's cone around the server's aim. */
	OutsideCone,
	/** Farther than the weapon reaches. */
	OutOfRange,
	/** The impact is nowhere near the target, even where it may have been during the owner's latency. */
	NotOnTarget,
	/** Something solid stands between the server's eyes and the impact. */
	Blocked
};

/** What the server knows about one claimed pellet hit (HollowWeaponMath::CheckHit). */
struct FHollowShotCheck
{
	/** Where the server sees the shooter's eyes and aim. */
	FVector ServerOrigin = FVector::ZeroVector;
	FVector ServerAim = FVector::ForwardVector;

	/** What the owner claims: its eyes, the pellet's direction and where it landed. */
	FVector ClaimedOrigin = FVector::ZeroVector;
	FVector ClaimedDirection = FVector::ForwardVector;
	FVector ImpactPoint = FVector::ZeroVector;

	/** The target as the server has it now, and how fast it moves. */
	FBox TargetBounds = FBox(ForceInit);
	FVector TargetVelocity = FVector::ZeroVector;

	/** Half the owner's round trip, capped by MaxRewindSeconds (the target may have moved that long). */
	float LatencySeconds = 0.f;
	float MaxRewindSeconds = 0.25f;

	float Range = 0.f;
	float SpreadDegrees = 0.f;
	float AimToleranceDegrees = 0.f;
	float OriginToleranceCm = 0.f;
	float BoundsToleranceCm = 0.f;

	/** The server's trace from its eyes to the impact met nothing solid on the way. */
	bool bLineOfSight = true;
};

/** Pure weapon math (GP-01, ADR-0044; covered by Hollow.GAS.Weapons.Math). */
namespace HollowWeaponMath
{
	/**
	 * Directions of one shot's pellets inside a cone of SpreadDegrees around Forward, the same for the same Seed on every
	 * machine (the owner's hits, the others' tracers). One pellet without spread flies straight.
	 */
	HOLLOWGAS_API void PelletDirections(const FVector& Forward, int32 Pellets, float SpreadDegrees, int32 Seed, TArray<FVector>& OutDirections);

	/** Base damage at Distance: full up to FalloffStart, then down to MinFraction of it at FalloffEnd and beyond. */
	HOLLOWGAS_API float DamageAtDistance(float Base, float Distance, float FalloffStart, float FalloffEnd, float MinFraction);

	/** The server's verdict on one of the owner's pellet hits, checks in the order of EHollowShotVerdict. */
	HOLLOWGAS_API EHollowShotVerdict CheckHit(const FHollowShotCheck& Check);

	/** Rounds a reload puts in: what the magazine lacks, at most what is carried (never negative). */
	HOLLOWGAS_API int32 RoundsToLoad(int32 Magazine, int32 MagazineSize, int32 Carried);

	HOLLOWGAS_API const TCHAR* LexVerdict(EHollowShotVerdict Verdict);
}
