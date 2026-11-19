#pragma once

#include "Engine/DataAsset.h"

#include "HollowMovementTuning.generated.h"

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
};
