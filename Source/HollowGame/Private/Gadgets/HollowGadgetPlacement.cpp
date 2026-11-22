#include "Gadgets/HollowGadgetPlacement.h"

#include "Components/CapsuleComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

FTransform HollowGadgetPlacement::OnFloorBelow(const ACharacter& Placer, const float TraceDistance)
{
	const FVector Middle = Placer.GetActorLocation();
	const FVector Feet = Middle - FVector(0.0, 0.0, Placer.GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	FHitResult Floor;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(HollowGadgetPlace), /*bTraceComplex=*/false, &Placer);
	const bool bFloor = Placer.GetWorld()->LineTraceSingleByChannel(Floor, Middle, Feet - FVector(0.0, 0.0, TraceDistance), ECC_Visibility, Params);
	return FTransform(FRotator(0.0, Placer.GetActorRotation().Yaw, 0.0), bFloor ? Floor.ImpactPoint : Feet);
}
