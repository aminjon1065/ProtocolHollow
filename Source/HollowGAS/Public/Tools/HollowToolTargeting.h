#pragma once

#include "CoreMinimal.h"

#include "HollowToolTargeting.generated.h"

/** What one server sample of a continuous tool works on. */
UENUM()
enum class EHollowToolAimResult : uint8
{
	/** The trace hit a tool target: work on it (it becomes the current target). */
	WorkOnHit,
	/** Missed, but the current target is still within the sticky angle and grace time: keep working on it. */
	KeepCurrent,
	/** Nothing to work on. */
	Lose
};

/** Facts of one sample, measured on the server. Plain data, so the rules are unit-testable. */
struct FHollowToolAim
{
	bool bHitTarget = false;
	bool bHasCurrent = false;
	/** Between the view direction and the direction to the current target's last impact point. */
	float AngleToCurrentDegrees = 0.f;
	double SecondsSinceCurrentHit = 0.0;
};

namespace HollowToolTargeting
{
	/**
	 * Target stickiness (Docs/Architecture/03 sec. 3.2): without it a weld at the edge of a door would flicker at
	 * 100+ ms ping. Both limits are inclusive.
	 */
	HOLLOWGAS_API EHollowToolAimResult Resolve(const FHollowToolAim& Aim, float MaxAngleDegrees, float GraceSeconds);
}
