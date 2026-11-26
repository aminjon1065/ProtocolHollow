#pragma once

#include "CoreMinimal.h"

#include "HollowCarryTypes.generated.h"

class APawn;

/** How a heavy object is carried (Docs/Architecture/03 sec. 3.5). Solo arrives in S3-02, Coop in S3-06; Dragged is reserved. */
UENUM(BlueprintType)
enum class EHollowCarryMode : uint8
{
	None,
	Solo,
	Coop,
	Drone,
	Winched,
	Dragged
};

/**
 * Carry slowdown a carrier's saved moves claim (three compressed move flags, HollowMoveFlags::Carry). The server
 * accepts any claim that is at least as slow as its own state (HollowCarryPolicy), so speed changes never correct.
 */
UENUM(BlueprintType)
enum class EHollowCarryClaim : uint8
{
	None,
	Solo,
	Coop,
	Dragged,
	/** DRONE-01 (ADR-0054): the player's solo drone carries the load beside it; the player keeps pace with it. */
	Drone
};

/**
 * Replicated carry state of a carried object (ADR-0015): mode and carriers. While carried, the object's transform is
 * a function of the carriers on every machine and is not replicated.
 */
USTRUCT()
struct HOLLOWPHYSICS_API FHollowCarryNetState
{
	GENERATED_BODY()

	UPROPERTY()
	EHollowCarryMode Mode = EHollowCarryMode::None;

	/** Solo carrier, or the first one in Coop. */
	UPROPERTY()
	TObjectPtr<APawn> Primary = nullptr;

	/** Second carrier in Coop (S3-06, ADR-0028): the player who joined an already carried object. */
	UPROPERTY()
	TObjectPtr<APawn> Secondary = nullptr;

	/**
	 * DRONE-01 (ADR-0054): in Drone mode, what holds the load for Primary (its solo drone, an IHollowCarryVehicle);
	 * the object hangs from it on every machine. Primary still counts as the carrier (the altar, the extraction).
	 */
	UPROPERTY()
	TObjectPtr<AActor> Vehicle = nullptr;

	/** Increments on every change, so a repeated pick-up still replicates. */
	UPROPERTY()
	uint8 TransitionSerial = 0;

	bool IsCarried() const { return Mode != EHollowCarryMode::None; }
	bool IsCarriedBy(const APawn* Pawn) const { return Pawn && IsCarried() && (Primary == Pawn || Secondary == Pawn); }
};

/** Why a carrier stops carrying (server; logs and tests, ADR-0028). */
UENUM()
enum class EHollowCarryLeave : uint8
{
	/** The carrier pressed Interact. */
	Released,
	/** The carrier is going away: died, left the game, unpossessed. */
	Gone,
	/** The two carriers drifted further apart than the tether allows. */
	TetherBroken,
	/** The server's sanity check refused the carrier's own movement (too fast, inside the world). */
	Violation
};

namespace HollowCarry
{
	/** The slowdown a carrier in Mode claims: the drone's own (DRONE-01); a winched object has no carrier. */
	HOLLOWPHYSICS_API EHollowCarryClaim ClaimFor(EHollowCarryMode Mode);
}
