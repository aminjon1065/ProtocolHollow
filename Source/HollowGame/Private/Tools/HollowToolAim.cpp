#include "Tools/HollowToolAim.h"

#include "Data/HollowToolDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Tools/HollowToolMountComponent.h"

FHollowToolAimTrace HollowToolAim::TraceLocal(const APawn& Pawn, const UHollowToolDefinition& Tool)
{
	FHollowToolAimTrace Trace;
	Trace.Eyes = Pawn.GetPawnViewLocation();
	Trace.Aim = Pawn.GetBaseAimRotation();
	Trace.End = Trace.Eyes + Trace.Aim.Vector() * Tool.Range;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(HollowToolAim), /*bTraceComplex=*/false, &Pawn);
	Trace.bHit = Pawn.GetWorld()->LineTraceSingleByChannel(Trace.Hit, Trace.Eyes, Trace.End, Tool.TraceChannel, Params);
	if (Trace.bHit)
	{
		Trace.End = Trace.Hit.ImpactPoint;
	}
	return Trace;
}

FVector HollowToolAim::BeamStart(const APawn& Pawn, const FHollowToolAimTrace& Trace, const FVector& FallbackOffset)
{
	FVector Muzzle;
	const UHollowToolMountComponent* Mount = UHollowToolMountComponent::Get(&Pawn);
	if (Mount && Mount->GetMuzzleLocation(Muzzle))
	{
		return Muzzle;
	}
	return Trace.Eyes + Trace.Aim.RotateVector(FallbackOffset);
}
