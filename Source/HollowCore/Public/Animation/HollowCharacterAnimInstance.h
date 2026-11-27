#pragma once

#include "Animation/AnimInstance.h"

#include "HollowCharacterAnimInstance.generated.h"

/** How a character moves this frame, as its animation blueprint reads it (CHAR-01, ADR-0055). */
struct HOLLOWCORE_API FHollowLocomotion
{
	/** Ground speed (cm/s): the velocity across the floor. */
	float Speed = 0.f;
	/** Where it moves relative to where it faces (degrees, -180..180: 0 forward, 90 to its right). 0 standing. */
	float Direction = 0.f;
	/** Where it looks up (+) or down (-) relative to its body (degrees, -90..90). */
	float AimPitch = 0.f;
	bool bFalling = false;

	/**
	 * From the character's velocity, the rotation it faces and the one it aims (its view's for a player). Below
	 * StandingSpeed it stands: no direction (a turn in place keeps the legs still).
	 */
	static FHollowLocomotion From(const FVector& Velocity, const FRotator& Facing, const FRotator& Aim, bool bFalling);

	static constexpr float StandingSpeed = 3.f;
	static constexpr float MaxAimPitch = 90.f;
};

/**
 * Base of every character's animation blueprint (CHAR-01, ADR-0055: ABP_ExoFrame_Body/Arms, ABP_IronPilgrim,
 * ABP_EchoStalker). It reads the owning pawn once per animation update and exposes what the graph blends by: speed and
 * direction for the locomotion blend space, the aim pitch for an aim offset. The graph itself (blend space, aim offset,
 * the DefaultSlot the montages play in) is the blueprint's (create_character_anim_assets.py). Presentation only:
 * nothing here moves the body (no root motion, ADR-0027) or replicates.
 */
UCLASS(Blueprintable)
class HOLLOWCORE_API UHollowCharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	const FHollowLocomotion& GetLocomotion() const { return Locomotion; }

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** Ground speed, cm/s (FHollowLocomotion::Speed). */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Hollow|Locomotion")
	float Speed = 0.f;

	/** Movement direction relative to the facing, degrees (FHollowLocomotion::Direction). */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Hollow|Locomotion")
	float Direction = 0.f;

	/** Look up (+) or down (-), degrees (FHollowLocomotion::AimPitch). */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Hollow|Locomotion")
	float AimPitch = 0.f;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Hollow|Locomotion")
	bool bIsFalling = false;

private:
	FHollowLocomotion Locomotion;
};
