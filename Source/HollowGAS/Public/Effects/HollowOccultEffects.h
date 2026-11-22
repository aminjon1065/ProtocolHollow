#pragma once

#include "GameplayEffect.h"

#include "HollowOccultEffects.generated.h"

/**
 * Structure shared by the Aether lantern's effects (S6-02, ADR-0035): a short duration (data) that every lantern sample
 * renews while the target stays in the light or the aura, so it lingers that long once it leaves; one instance per
 * target whoever applies it (two lanterns do not slow twice), a renewal triggering no cue again (no RPC per sample);
 * its state tag on the target. Modifiers are data.
 */
UCLASS(Abstract)
class HOLLOWGAS_API UHollowLanternEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	explicit UHollowLanternEffect(const FObjectInitializer& ObjectInitializer);

protected:
	/** Constructor only: grants StateTag. */
	void InitLanternStructure(const FGameplayTag& StateTag);
};

/**
 * GE_Occult_Expose: an enemy in the lantern's light. Hollow.State.Exposed (the Echo-Stalker flees and its kinetic
 * immunity is off, GE_Stalker_Traits) and its glow (GameplayCue.Hollow.Status.Exposed); the slow (MoveSpeedMultiplier)
 * and the weaker armour are modifiers of the BP child.
 */
UCLASS(Abstract)
class HOLLOWGAS_API UHollowOccultExposeEffect : public UHollowLanternEffect
{
	GENERATED_BODY()

public:
	explicit UHollowOccultExposeEffect(const FObjectInitializer& ObjectInitializer);
};

/**
 * GE_AntiResonance: a player within a lantern's aura. Hollow.State.AntiResonance; the lower ResonanceGainMultiplier
 * (every gain goes through it, UHollowResonanceGainCalculation) is a modifier of the BP child.
 */
UCLASS(Abstract)
class HOLLOWGAS_API UHollowAntiResonanceEffect : public UHollowLanternEffect
{
	GENERATED_BODY()

public:
	explicit UHollowAntiResonanceEffect(const FObjectInitializer& ObjectInitializer);
};
