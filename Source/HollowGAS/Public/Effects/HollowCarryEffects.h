#pragma once

#include "GameplayEffect.h"

#include "HollowCarryEffects.generated.h"

/** Shared structure of the heavy-carry effects: infinite while carried, a state tag, every tool and weapon blocked (both hands). */
UCLASS(Abstract)
class HOLLOWGAS_API UHollowCarryEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	explicit UHollowCarryEffect(const FObjectInitializer& ObjectInitializer);

protected:
	/** Constructor only: grants StateTag and blocks Hollow.Ability.Tool. */
	void InitCarryStructure(const FGameplayTag& StateTag);
};

/**
 * Structure of the solo heavy-carry effect (ADR-0015): infinite while carried, grants Hollow.State.CarryingHeavy.Solo
 * and blocks every tool ability (both hands are full). Period, stamina drain and Resonance gain are data
 * (GE_Carry_Solo derives from this). Sprint and jump are not abilities: the carrier's saved moves forbid them.
 */
UCLASS(Abstract)
class HOLLOWGAS_API UHollowCarrySoloEffect : public UHollowCarryEffect
{
	GENERATED_BODY()

public:
	explicit UHollowCarrySoloEffect(const FObjectInitializer& ObjectInitializer);
};

/**
 * Structure of the co-op heavy-carry effect (ADR-0028), on each of two carriers instead of the solo one: infinite while
 * carried, grants Hollow.State.CarryingHeavy.Coop and blocks every tool ability. Period, the lighter stamina drain and
 * the Resonance gain are data (GE_Carry_Coop derives from this).
 */
UCLASS(Abstract)
class HOLLOWGAS_API UHollowCarryCoopEffect : public UHollowCarryEffect
{
	GENERATED_BODY()

public:
	explicit UHollowCarryCoopEffect(const FObjectInitializer& ObjectInitializer);
};

/**
 * Structure of the passive stamina recovery: infinite, paused while the player carries anything heavy
 * (Hollow.State.CarryingHeavy). Period and amount are data (GE_Stamina_Recovery derives from this).
 */
UCLASS(Abstract)
class HOLLOWGAS_API UHollowStaminaRecoveryEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	explicit UHollowStaminaRecoveryEffect(const FObjectInitializer& ObjectInitializer);
};
