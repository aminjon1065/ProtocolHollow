#include "Weapons/HollowGA_WeaponAim.h"

#include "AbilitySystemComponent.h"
#include "Attributes/HollowWeaponSet.h"
#include "Audio/HollowAudioSubsystem.h"
#include "Tags/HollowGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowGA_WeaponAim)

UHollowGA_WeaponAim::UHollowGA_WeaponAim(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EHollowAbilityActivationPolicy::WhileInputActive;
	SetAssetTags(FGameplayTagContainer(HollowTags::Ability_Weapon_Aim));
	ActivationOwnedTags.AddTag(HollowTags::State_Weapon_Aiming);   // local and immediate under prediction
	ActivationRequiredTags.AddTag(HollowTags::State_Hands_Weapon);
	ActivationBlockedTags.AddTag(HollowTags::State_Dead);
	ActivationBlockedTags.AddTag(HollowTags::State_Hands_Swapping);
	ActivationBlockedTags.AddTag(HollowTags::State_Weapon_Reloading);
}

void UHollowGA_WeaponAim::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
	}
	// Held until the input is released (UHollowAbilitySystemComponent cancels WhileInputActive abilities then).
}

UHollowGA_WeaponDryFire::UHollowGA_WeaponDryFire(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EHollowAbilityActivationPolicy::OnInputTriggered;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
	ActivationRequiredTags.AddTag(HollowTags::State_Hands_Weapon);
	ActivationBlockedTags.AddTag(HollowTags::State_Dead);
	ActivationBlockedTags.AddTag(HollowTags::State_Hands_Swapping);
	ActivationBlockedTags.AddTag(HollowTags::State_Weapon_Reloading);
	ActivationBlockedTags.AddTag(HollowTags::Cooldown_Weapon_Fire);   // the press that fired the last round clicks no more
}

bool UHollowGA_WeaponDryFire::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	const UAbilitySystemComponent* AbilitySystem = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	return AbilitySystem && AbilitySystem->GetNumericAttribute(UHollowWeaponSet::GetMagazineAttribute()) < 1.f
		&& Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UHollowGA_WeaponDryFire::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(GetAvatarActorFromActorInfo()))
	{
		Audio->PlayAttached(HollowTags::Sound_Weapon_DryFire, GetAvatarActorFromActorInfo()->GetRootComponent());
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/false, /*bWasCancelled=*/false);
	// Then the reload, if there is anything to load (its own prediction).
	ActorInfo->AbilitySystemComponent->TryActivateAbilitiesByTag(FGameplayTagContainer(HollowTags::Ability_Weapon_Reload));
}
