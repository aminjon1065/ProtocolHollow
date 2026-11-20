#pragma once

#include "GameplayEffect.h"

#include "HollowCarryEffects.generated.h"

/**
 * Structure of the solo heavy-carry effect (ADR-0015): infinite while carried, grants Hollow.State.CarryingHeavy.Solo
 * and blocks every tool ability (both hands are full). Period, stamina drain and Resonance gain are data
 * (GE_Carry_Solo derives from this). Sprint and jump are not abilities: the carrier's saved moves forbid them.
 */
UCLASS(Abstract)
class HOLLOWGAS_API UHollowCarrySoloEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	explicit UHollowCarrySoloEffect(const FObjectInitializer& ObjectInitializer);
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
