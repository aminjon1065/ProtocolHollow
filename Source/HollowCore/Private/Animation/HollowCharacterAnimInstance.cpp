#include "Animation/HollowCharacterAnimInstance.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowCharacterAnimInstance)

FHollowLocomotion FHollowLocomotion::From(const FVector& Velocity, const FRotator& Facing, const FRotator& Aim, const bool bInFalling)
{
	FHollowLocomotion Result;
	const FVector Ground(Velocity.X, Velocity.Y, 0.0);
	Result.Speed = static_cast<float>(Ground.Size());
	if (Result.Speed >= StandingSpeed)
	{
		Result.Direction = static_cast<float>(FMath::FindDeltaAngleDegrees(Facing.Yaw, Ground.Rotation().Yaw));
	}
	const double Pitch = FRotator::NormalizeAxis(Aim.Pitch - Facing.Pitch);
	Result.AimPitch = static_cast<float>(FMath::Clamp(Pitch, -static_cast<double>(MaxAimPitch), static_cast<double>(MaxAimPitch)));
	Result.bFalling = bInFalling;
	return Result;
}

void UHollowCharacterAnimInstance::NativeUpdateAnimation(const float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	const APawn* Pawn = TryGetPawnOwner();
	if (!Pawn)
	{
		return;   // the editor's preview: it stands
	}
	const UPawnMovementComponent* Movement = Pawn->GetMovementComponent();
	Locomotion = FHollowLocomotion::From(Pawn->GetVelocity(), Pawn->GetActorRotation(), Pawn->GetBaseAimRotation(), Movement && Movement->IsFalling());
	Speed = Locomotion.Speed;
	Direction = Locomotion.Direction;
	AimPitch = Locomotion.AimPitch;
	bIsFalling = Locomotion.bFalling;
}
