#pragma once

#include "Abilities/HollowGameplayAbility.h"

#include "HollowGA_EnemyAttack.generated.h"

class APawn;
class UHollowEnemyAttackDefinition;

/**
 * Base of enemy attacks (ADR-0017): server only, triggered by a gameplay event whose payload target is what to hit.
 * The attacker stops and faces the target, the wind-up cue plays (replicated, the telegraph players react to), the
 * blow lands when the wind-up ends (Strike), and the attack ends after the recovery. Timings live in the attack
 * definition; ability tasks time it (no Tick). Animations and motion warping come with the art (montage + notify).
 */
UCLASS(Abstract)
class HOLLOWAI_API UHollowGA_EnemyAttack : public UHollowGameplayAbility
{
	GENERATED_BODY()

public:
	explicit UHollowGA_EnemyAttack(const FObjectInitializer& ObjectInitializer);

	/** The gameplay event that triggers it (the brain sends it with the target). */
	const FGameplayTag& GetTriggerEvent() const { return TriggerEvent; }
	const UHollowEnemyAttackDefinition* GetAttack() const { return Attack; }

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Can the attack start against Target? */
	virtual bool CanStrike(const AActor* Target) const { return Target != nullptr; }
	/** Server, when the wind-up ends: lands the blow on Target (it may have moved away meanwhile). */
	virtual void Strike(AActor* Target) PURE_VIRTUAL(UHollowGA_EnemyAttack::Strike, );

	/** Constructor helper of the children: the event that triggers the attack and the tag that names it. */
	void SetTrigger(const FGameplayTag& Event, const FGameplayTag& AbilityTag);

	APawn* GetAttacker() const;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Attack")
	TObjectPtr<UHollowEnemyAttackDefinition> Attack;

private:
	UFUNCTION()
	void HandleWindupFinished();

	UFUNCTION()
	void HandleRecoveryFinished();

	void RemoveWindupCue();

	UPROPERTY(VisibleDefaultsOnly, Category = "Hollow|Attack")
	FGameplayTag TriggerEvent;

	TWeakObjectPtr<AActor> CurrentTarget;
	bool bWindupCueActive = false;
};
