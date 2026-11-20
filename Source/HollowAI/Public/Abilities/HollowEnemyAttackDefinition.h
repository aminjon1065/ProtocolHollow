#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "HollowEnemyAttackDefinition.generated.h"

class UGameplayEffect;

/** Tuning of one enemy attack (DA_Attack_*), read by UHollowGA_EnemyAttack and its children (ADR-0017). */
UCLASS(BlueprintType)
class HOLLOWAI_API UHollowEnemyAttackDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Telegraph before the blow lands (the wind-up cue plays meanwhile): the time players have to get away. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.01", Units = "Seconds"))
	float WindupSeconds = 0.f;

	/** After the blow, before it can act again. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.01", Units = "Seconds"))
	float RecoverySeconds = 0.f;

	/** Reach in front of the attacker's capsule edge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reach", meta = (ClampMin = "0", Units = "Centimeters"))
	float Reach = 0.f;

	/** Radius of the melee sweep. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reach", meta = (ClampMin = "0", Units = "Centimeters"))
	float Radius = 0.f;

	/** Melee damage (SetByCaller Hollow.Data.Damage of DamageEffect). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0"))
	float Damage = 0.f;

	/** Effect that carries the melee damage (GE_Damage_Kinetic: UHollowDamageExecution). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	TSubclassOf<UGameplayEffect> DamageEffect;

	/** Structural damage of one blow at a door or bulkhead (IHollowStructural). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0"))
	float StructuralDamage = 0.f;

	/** Cue on the attacker while it winds up (replicated: every player sees the blow coming). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (Categories = "GameplayCue.Hollow"))
	FGameplayTag WindupCue;
};
