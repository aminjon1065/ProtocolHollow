#include "Tools/HollowToolAim.h"

#include "Data/HollowToolDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

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
