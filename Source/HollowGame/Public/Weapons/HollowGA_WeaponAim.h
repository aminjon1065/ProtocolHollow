#pragma once

#include "Abilities/HollowGameplayAbility.h"

#include "HollowGA_WeaponAim.generated.h"

/**
 * Aims the weapon in hand while the secondary input is held (GP-01): Hollow.State.Weapon.Aiming, which tightens the
 * spread (UHollowWeaponDefinition::AimSpreadMultiplier). The tools keep the secondary input in their mode (the welder).
 */
UCLASS()
class HOLLOWGAME_API UHollowGA_WeaponAim : public UHollowGameplayAbility
{
	GENERATED_BODY()

public:
	explicit UHollowGA_WeaponAim(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

/**
 * The trigger of an empty weapon (GP-01): a dry click on the owner and, with rounds in the raid inventory, the reload.
 * On the primary input next to the fire ability; only while the magazine is empty and no shot just went off. Local only.
 */
UCLASS()
class HOLLOWGAME_API UHollowGA_WeaponDryFire : public UHollowGameplayAbility
{
	GENERATED_BODY()

public:
	explicit UHollowGA_WeaponDryFire(const FObjectInitializer& ObjectInitializer);

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
