#pragma once

#include "CoreMinimal.h"

#include "HollowInteractionRules.generated.h"

/** Why the server refused an interaction request. */
UENUM()
enum class EHollowInteractionRejection : uint8
{
	None,
	NoTarget,
	NotInteractable,
	InteractorBlocked,
	TooSoon,
	OutOfRange,
	NoLineOfSight,
	TargetRefused
};

/** Facts the server measured for one interaction request. Plain data, so the rules are unit-testable. */
struct FHollowInteractionCheck
{
	bool bTargetValid = false;
	bool bTargetInteractable = false;
	/** The interacting character owns one of the blocking state tags (dead, downed, stunned...). */
	bool bInteractorBlocked = false;
	/** Seconds since this player's last accepted interaction; unset if there was none. */
	TOptional<double> SecondsSinceLastInteraction;
	/** Distance from the eyes to the target along the server's line-of-sight trace. */
	float Distance = 0.f;
	bool bHasLineOfSight = false;
	/** IHollowInteractable::CanInteract on the server. */
	bool bTargetAllows = false;
};

namespace HollowInteractionRules
{
	/** Server decision for an interaction request (CLAUDE.md sec. 3: validated and executed on the server). */
	HOLLOWGAME_API EHollowInteractionRejection ValidateRequest(const FHollowInteractionCheck& Check, float MaxDistance, float MinInterval);
}
