#include "AbilitySystem/HollowAbilitySystemComponent.h"

#include "Abilities/HollowGameplayAbility.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowAbilitySystemComponent)

void UHollowAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	const AActor* PreviousAvatar = AbilityActorInfo.IsValid() ? AbilityActorInfo->AvatarActor.Get() : nullptr;
	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);

	if (InAvatarActor && InAvatarActor != PreviousAvatar)
	{
		TryActivateAbilitiesOnSpawn();
	}
}

void UHollowAbilitySystemComponent::TryActivateAbilitiesOnSpawn()
{
	ABILITYLIST_SCOPE_LOCK();
	for (const FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (const UHollowGameplayAbility* Ability = Cast<UHollowGameplayAbility>(Spec.Ability))
		{
			Ability->TryActivateOnSpawn(AbilityActorInfo.Get(), Spec);
		}
	}
}
