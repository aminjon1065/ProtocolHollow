#pragma once

#include "CoreMinimal.h"

#include "HollowCarryCoop.generated.h"

/**
 * How a heavy object is carried by two (ADR-0028), part of its UHollowCarryableDefinition: the carriers' spacing and
 * the tether that keeps them at it. Speeds are not here: the carriers' movement tuning has them per mode (ADR-0015).
 */
USTRUCT(BlueprintType)
struct HOLLOWPHYSICS_API FHollowCoopCarryTuning
{
	GENERATED_BODY()

	/** Horizontal distance between the two carriers' capsule centres the object is held at. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Coop", meta = (ClampMin = "1", Units = "Centimeters"))
	float CarrierSpacing = 1.f;

	/** The tether is slack while the spacing is within this much of CarrierSpacing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Coop", meta = (ClampMin = "0", Units = "Centimeters"))
	float Slack = 0.f;

	/** Beyond the slack a carrier is pulled towards its partner at this speed per centimetre of stretch. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Coop", meta = (ClampMin = "0"))
	float PullRate = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Coop", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float MaxPullSpeed = 0.f;

	/** Carriers further apart than CarrierSpacing plus this (3D) lose the object (server sanity check). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Coop", meta = (ClampMin = "0", Units = "Centimeters"))
	float BreakDistance = 0.f;

	/** The furthest the carriers may be apart and still carry (inclusive). */
	float GetMaxSeparation() const { return CarrierSpacing + BreakDistance; }
};

/** Pose of an object carried by two (ADR-0028 "virtual stretcher"): a function of the two carriers only. */
namespace HollowCarryCoop
{
	/**
	 * Midway between the carriers' grips (GripHeight up their capsules), its X axis along the ground from Primary to
	 * Secondary (Primary's facing if they stand on one spot). Every machine computes it: the server from the capsules
	 * (the truth: zones, perception), every machine from the bodies it renders for the visual.
	 */
	HOLLOWPHYSICS_API FTransform Pose(const FTransform& Primary, const FTransform& Secondary, double GripHeight);
}

/** The tether between two carriers (ADR-0028), applied by whoever controls a carrier (owning client, listen host). */
namespace HollowCarryTether
{
	/**
	 * Velocity of a carrier at Self whose partner stands at Partner (capsule centres; along the ground only, Z kept):
	 * - stretched beyond CarrierSpacing + Slack: it moves towards the partner at least at the pull speed
	 *   (PullRate per centimetre of stretch, at most MaxPullSpeed), so it never walks away;
	 * - squeezed under CarrierSpacing - Slack: it no longer moves towards the partner;
	 * - never faster than MaxSpeed along the ground.
	 */
	HOLLOWPHYSICS_API FVector Constrain(const FVector& Velocity, const FVector& Self, const FVector& Partner, const FHollowCoopCarryTuning& Tuning,
		float MaxSpeed);

	/** Whether carriers at A and B are too far apart to carry (3D, beyond GetMaxSeparation). */
	HOLLOWPHYSICS_API bool IsBroken(const FVector& A, const FVector& B, const FHollowCoopCarryTuning& Tuning);
}

/** The server's sanity check of client-authoritative carriers (ADR-0028, every CoopSanityIntervalSeconds). */
namespace HollowCoopSanity
{
	/** Whether moving From -> To in Seconds is faster along the ground than MaxSpeed x Tolerance (the limit itself passes). */
	HOLLOWPHYSICS_API bool IsTooFast(const FVector& From, const FVector& To, double Seconds, float MaxSpeed, float Tolerance);

	/**
	 * The time a carrier moved for between two checks: the longer of the server's interval and the client's own (its
	 * move timestamps). Packets arrive with jitter, so the newest position the server holds at one check can be 100 ms
	 * older than at the next: by the server's clock alone an honest carrier looks too fast. A client interval that is
	 * not positive (a timestamp reset) is ignored.
	 */
	HOLLOWPHYSICS_API double MovedFor(double ServerSeconds, double ClientSeconds);

	/**
	 * Which carrier lets go when the tether breaks: the one that moved further since the last check (it walked
	 * away); the secondary on a tie, so the object stays with whoever lifted it.
	 */
	HOLLOWPHYSICS_API bool DoesSecondaryLetGo(double PrimaryMoved, double SecondaryMoved);
}

/**
 * When a client-authoritative carrier that left a co-op carry is server-authoritative again (ADR-0028). The server
 * cannot switch back at the leave itself: until the client learns it, it still applies the tether, and its velocity
 * differs from the one the server simulated. Back once the client knows (a move without the Coop claim after one with
 * it), SettleSeconds have passed since (both sides then move from the same inputs without a tether) and the server's
 * simulation of the move lands where the client did; or after TimeoutSeconds whatever happened.
 */
struct HOLLOWPHYSICS_API FHollowCoopReturn
{
	/** Server time of the leave. */
	double LeftAt = 0.0;
	/** Server time of the first move that showed the client knows; unset until then. */
	TOptional<double> KnownAt;

	bool ShouldReturn(double Now, bool bInSync, float SettleSeconds, float TimeoutSeconds) const;
};
