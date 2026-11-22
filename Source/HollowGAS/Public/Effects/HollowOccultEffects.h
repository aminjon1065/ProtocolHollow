#pragma once

#include "GameplayEffect.h"

#include "HollowOccultEffects.generated.h"

/**
 * Structure of the effects an area keeps on whoever is in it (S6-02 lantern, S6-03 ward; ADR-0035, ADR-0036): a short
 * duration (data) that every check of the area renews while the target stays in, so it lingers that long once it
 * leaves; one instance per target whoever applies it (two lanterns or wards do not stack), a renewal triggering no cue
 * again (no RPC per check); its state tag on the target. Modifiers are data.
 */
UCLASS(Abstract)
class HOLLOWGAS_API UHollowRenewedEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	explicit UHollowRenewedEffect(const FObjectInitializer& ObjectInitializer);

protected:
	/** Constructor only: grants StateTag. */
	void InitRenewedStructure(const FGameplayTag& StateTag);
};

/** The Aether lantern's effects (S6-02): renewed by every lantern sample. */
UCLASS(Abstract)
class HOLLOWGAS_API UHollowLanternEffect : public UHollowRenewedEffect
{
	GENERATED_BODY()

public:
	explicit UHollowLanternEffect(const FObjectInitializer& ObjectInitializer);
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

/**
 * GE_Ward_Bind: an enemy inside a ritual ward (S6-03). Hollow.State.Warded; the slow (MoveSpeedMultiplier, on top of
 * the lantern's) is a modifier of the BP child.
 */
UCLASS(Abstract)
class HOLLOWGAS_API UHollowWardBindEffect : public UHollowRenewedEffect
{
	GENERATED_BODY()

public:
	explicit UHollowWardBindEffect(const FObjectInitializer& ObjectInitializer);
};

/**
 * GE_Ward_Calm: a player inside a ritual ward (S6-03). Hollow.State.Warded; the lower ResonanceGainMultiplier is a
 * modifier of the BP child (the drain of Resonance is the ward's own instant effect, applied with each check: a
 * periodic effect would turn this multiplier into a repeated base change).
 */
UCLASS(Abstract)
class HOLLOWGAS_API UHollowWardCalmEffect : public UHollowRenewedEffect
{
	GENERATED_BODY()

public:
	explicit UHollowWardCalmEffect(const FObjectInitializer& ObjectInitializer);
};
