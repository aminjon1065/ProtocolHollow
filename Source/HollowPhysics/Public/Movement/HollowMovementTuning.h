#pragma once

#include "Carry/HollowCarryTypes.h"
#include "Engine/DataAsset.h"

#include "HollowMovementTuning.generated.h"

/** Movement of a character while it carries a heavy object in one mode (ADR-0015: per mode, decided by the saved moves). */
USTRUCT(BlueprintType)
struct FHollowCarryModeTuning
{
	GENERATED_BODY()

	/** Multiplier of the walking (or crouched) max speed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry", meta = (ClampMin = "0.05", ClampMax = "1"))
	float SpeedScale = 1.f;

	/** Multiplier of the max acceleration: a heavy load starts and turns slowly. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry", meta = (ClampMin = "0.05", ClampMax = "1"))
	float AccelerationScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry")
	bool bAllowSprint = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry")
	bool bAllowJump = false;
};

/** Carry movement of a character class; a claim of None moves freely (speed and acceleration scale 1). */
USTRUCT(BlueprintType)
struct HOLLOWPHYSICS_API FHollowCarryMovementTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry")
	FHollowCarryModeTuning Solo;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry")
	FHollowCarryModeTuning Coop;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry")
	FHollowCarryModeTuning Dragged;

	/** Tuning of a claim; null for None (no carry restrictions). */
	const FHollowCarryModeTuning* Find(EHollowCarryClaim Claim) const;

	float GetSpeedScale(EHollowCarryClaim Claim) const;
};

/** Ground speeds of a character. Plain struct so the speed model stays unit-testable (HollowMovementMath). */
USTRUCT(BlueprintType)
struct FHollowGroundSpeedTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float WalkSpeed = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float CrouchSpeed = 0.f;

	/** Multiplier applied to WalkSpeed while sprinting. 1 disables sprint. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sprint", meta = (ClampMin = "1"))
	float SprintSpeedMultiplier = 1.f;

	/** Sprint only while moving within this angle of the facing direction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sprint", meta = (ClampMin = "0", ClampMax = "90", Units = "Degrees"))
	float SprintMaxAngle = 0.f;
};

/**
 * Movement tuning of a character class (CLAUDE.md sec. 1: no movement numbers in C++).
 * Applied by UHollowCharacterMovementComponent when it initializes.
 */
UCLASS(BlueprintType, Const)
class HOLLOWPHYSICS_API UHollowMovementTuning : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground")
	FHollowGroundSpeedTuning Ground;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (ClampMin = "0", Units = "CentimetersPerSecondSquared"))
	float MaxAcceleration = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (ClampMin = "0", Units = "CentimetersPerSecondSquared"))
	float BrakingDeceleration = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (ClampMin = "0"))
	float GroundFriction = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float JumpZVelocity = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air", meta = (ClampMin = "0", ClampMax = "1"))
	float AirControl = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air", meta = (ClampMin = "0", Units = "CentimetersPerSecondSquared"))
	float BrakingDecelerationFalling = 0.f;

	/**
	 * Speed, acceleration, sprint and jump while carrying, per mode. Here and not on the carried object: the server
	 * applies a carrier's claimed mode (two move flags) before, or without, ever seeing the pick-up request.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry")
	FHollowCarryMovementTuning Carry;
};
