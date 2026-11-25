#pragma once

#include "Abilities/HollowGameplayAbility.h"

#include "HollowGA_WeaponReload.generated.h"

class UGameplayEffect;
class UHollowWeaponDefinition;

/**
 * Reloads the weapon in hand (GP-01, ADR-0044), granted by the weapon's item on the reload input (R; the gamepad's X
 * tapped). Committed at once: the rounds the magazine lacks, as many as the raid inventory holds of the weapon's
 * ammunition, go in through LoadEffect (predicted on the owner, Hollow.Data.AmmoLoaded) and the server takes them from
 * the inventory; the weapon then stays busy for its ReloadSeconds (Hollow.State.Weapon.Reloading: no shot, no swap).
 * Refused with a full magazine or nothing to load.
 */
UCLASS()
class HOLLOWGAME_API UHollowGA_WeaponReload : public UHollowGameplayAbility
{
	GENERATED_BODY()

public:
	explicit UHollowGA_WeaponReload(const FObjectInitializer& ObjectInitializer);

	/** Rounds a reload would put in now (0: nothing to do). */
	static int32 GetRoundsToLoad(const UHollowWeaponDefinition& Weapon, const FGameplayAbilityActorInfo& ActorInfo);

	/** The rounds of Weapon's ammunition the owner carries (its raid inventory; 0 without one). */
	static int32 GetCarriedRounds(const UHollowWeaponDefinition& Weapon, const AActor* Owner);

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Weapon")
	TObjectPtr<const UHollowWeaponDefinition> WeaponDefinition;

	/** Instant effect adding Hollow.Data.AmmoLoaded to Magazine (the weapon set clamps it to MaxMagazine). */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Weapon")
	TSubclassOf<UGameplayEffect> LoadEffect;

private:
	void FinishReload();

	FTimerHandle ReloadTimer;
};
