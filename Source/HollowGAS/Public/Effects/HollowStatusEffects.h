#pragma once

#include "GameplayEffect.h"

#include "HollowStatusEffects.generated.h"

/**
 * Stunned (GE_State_Stunned, ADR-0019): Hollow.State.Stunned for the effect's duration (data in the BP child), enemy
 * abilities blocked. The brain stands still until the tag goes.
 */
UCLASS(Abstract)
class HOLLOWGAS_API UHollowStunnedEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	explicit UHollowStunnedEffect(const FObjectInitializer& ObjectInitializer);
};

/** Pulse mine recharging (cooldown of GA_PulseMine): Hollow.Cooldown.Gadget.PulseMine for the duration (data). */
UCLASS(Abstract)
class HOLLOWGAS_API UHollowPulseMineCooldownEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	explicit UHollowPulseMineCooldownEffect(const FObjectInitializer& ObjectInitializer);
};

/** Ritual ward recharging (cooldown of GA_OccultWard, S6-03): Hollow.Cooldown.Gadget.OccultWard for the duration (data). */
UCLASS(Abstract)
class HOLLOWGAS_API UHollowOccultWardCooldownEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	explicit UHollowOccultWardCooldownEffect(const FObjectInitializer& ObjectInitializer);
};

/** Helmet lamp just switched (cooldown of GA_HelmetLamp, S4-08): Hollow.Cooldown.HelmetLamp for the duration (data), no strobing. */
UCLASS(Abstract)
class HOLLOWGAS_API UHollowHelmetLampCooldownEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	explicit UHollowHelmetLampCooldownEffect(const FObjectInitializer& ObjectInitializer);
};

/**
 * A shot just went off (cooldown of the weapon's fire ability, GP-01): Hollow.Cooldown.Weapon.Fire for the duration
 * (data: a little under the weapon's FireIntervalSeconds, the server's floor; the owner keeps the exact interval).
 */
UCLASS(Abstract)
class HOLLOWGAS_API UHollowWeaponFireCooldownEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	explicit UHollowWeaponFireCooldownEffect(const FObjectInitializer& ObjectInitializer);
};

/** A melee blow just went out (cooldown of GA_MeleeBash, GP-01): Hollow.Cooldown.Melee for the duration (data). */
UCLASS(Abstract)
class HOLLOWGAS_API UHollowMeleeCooldownEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	explicit UHollowMeleeCooldownEffect(const FObjectInitializer& ObjectInitializer);
};

/**
 * The hands are switching (cooldown of GA_SwapHands, GP-01): Hollow.State.Hands.Swapping for the duration (data), which
 * blocks the tools, the weapon and another switch for that moment.
 */
UCLASS(Abstract)
class HOLLOWGAS_API UHollowHandsSwapEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	explicit UHollowHandsSwapEffect(const FObjectInitializer& ObjectInitializer);
};

/** A fuel cell just went in (cooldown of GA_ToolRefuel, S5-01): Hollow.Cooldown.Tool.Refuel for the duration (data). */
UCLASS(Abstract)
class HOLLOWGAS_API UHollowToolRefuelCooldownEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	explicit UHollowToolRefuelCooldownEffect(const FObjectInitializer& ObjectInitializer);
};

/**
 * What the Echo-Stalker is (GE_Stalker_Traits, from its ability set): immune to kinetic damage, except while it stands
 * in the Aether lantern's light (Hollow.State.Exposed inhibits the effect, S6-02).
 */
UCLASS(Abstract)
class HOLLOWGAS_API UHollowStalkerTraitsEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	explicit UHollowStalkerTraitsEffect(const FObjectInitializer& ObjectInitializer);
};
