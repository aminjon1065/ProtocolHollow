#pragma once

#include "Engine/HitResult.h"

class APawn;
class UHollowToolDefinition;

/** A local tool trace: what a pawn's tool beam hits on this machine (cosmetics and UI, never authority). */
struct FHollowToolAimTrace
{
	FVector Eyes = FVector::ZeroVector;
	FRotator Aim = FRotator::ZeroRotator;
	/** The impact point, or the end of the tool's reach. */
	FVector End = FVector::ZeroVector;
	FHitResult Hit;
	bool bHit = false;
};

namespace HollowToolAim
{
	/**
	 * Line trace along the pawn's aim (owners: control rotation, others: replicated aim) for the tool's range and
	 * channel. The beam cue and the visor share it, so the bar always describes what the beam touches.
	 */
	HOLLOWGAME_API FHollowToolAimTrace TraceLocal(const APawn& Pawn, const UHollowToolDefinition& Tool);

	/**
	 * Where a pawn's beam starts on this machine (ADR-0027): the held tool's muzzle (UHollowToolMountComponent), or
	 * without one FallbackOffset from the eyes along the aim (forward, right, up).
	 */
	HOLLOWGAME_API FVector BeamStart(const APawn& Pawn, const FHollowToolAimTrace& Trace, const FVector& FallbackOffset);
}
