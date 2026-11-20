#pragma once

#include "GameplayEffect.h"
#include "GameplayEffectExecutionCalculation.h"

#include "HollowDamageExecution.generated.h"

/**
 * The one way gameplay damage reaches Health (CLAUDE.md sec. 4, ADR-0017): base damage from SetByCaller
 * Hollow.Data.Damage, the type from the effect (UHollowDamageEffect::DamageType), the target's Armor captured at
 * execution, immunity and armor rules from UHollowDamageSettings. Writes IncomingDamage; UHollowHealthSet applies it.
 */
UCLASS()
class HOLLOWGAS_API UHollowDamageExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UHollowDamageExecution();

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};

/** Instant damage effect through UHollowDamageExecution; BP children (GE_Damage_Kinetic) only pick the type. */
UCLASS()
class HOLLOWGAS_API UHollowDamageEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	explicit UHollowDamageEffect(const FObjectInitializer& ObjectInitializer);

	/** Damage type the execution looks up in UHollowDamageSettings. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hollow|Damage", meta = (Categories = "Hollow.Damage"))
	FGameplayTag DamageType;
};
