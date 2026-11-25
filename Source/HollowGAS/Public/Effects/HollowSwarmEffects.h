#pragma once

#include "Effects/HollowOccultEffects.h"

#include "HollowSwarmEffects.generated.h"

/**
 * GE_Whisperer_Lit: a Whisperer in a player's lamp or lantern beam (GP-02, ADR-0047). Hollow.State.Lit (it flinches
 * and backs out of the light: AHollowWhispererController); the slow (MoveSpeedMultiplier) is a modifier of the BP
 * child. Renewed by every light check of UHollowSwarmSubsystem while the beam stays on it, so it lingers that long.
 */
UCLASS(Abstract)
class HOLLOWGAS_API UHollowLitEffect : public UHollowRenewedEffect
{
	GENERATED_BODY()

public:
	explicit UHollowLitEffect(const FObjectInitializer& ObjectInitializer);
};

/**
 * GE_Whisperer_Frenzy: a Whisperer in the dark whose prey has no light (GP-02). Hollow.State.Frenzy (no limit to the
 * swarm's bites, a louder whisper); the haste (MoveSpeedMultiplier) is a modifier of the BP child. Renewed by every
 * light check while the dark lasts.
 */
UCLASS(Abstract)
class HOLLOWGAS_API UHollowFrenzyEffect : public UHollowRenewedEffect
{
	GENERATED_BODY()

public:
	explicit UHollowFrenzyEffect(const FObjectInitializer& ObjectInitializer);
};
