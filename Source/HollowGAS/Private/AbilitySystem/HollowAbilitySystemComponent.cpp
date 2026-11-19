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

void UHollowAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}
	TArray<FGameplayAbilitySpecHandle> ToActivate;
	{
		ABILITYLIST_SCOPE_LOCK();
		for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
		{
			const UHollowGameplayAbility* Ability = Cast<UHollowGameplayAbility>(Spec.Ability);
			if (!Ability || Ability->GetActivationPolicy() == EHollowAbilityActivationPolicy::OnSpawn || !Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
			{
				continue;
			}
			Spec.InputPressed = true;
			if (Spec.IsActive())
			{
				AbilitySpecInputPressed(Spec);
			}
			else
			{
				ToActivate.Add(Spec.Handle);
			}
		}
	}
	for (const FGameplayAbilitySpecHandle& Handle : ToActivate)
	{
		TryActivateAbility(Handle);
	}
}

void UHollowAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}
	TArray<FGameplayAbilitySpecHandle> ToCancel;
	{
		ABILITYLIST_SCOPE_LOCK();
		for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
		{
			const UHollowGameplayAbility* Ability = Cast<UHollowGameplayAbility>(Spec.Ability);
			if (!Ability || !Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
			{
				continue;
			}
			Spec.InputPressed = false;
			if (Spec.IsActive())
			{
				AbilitySpecInputReleased(Spec);
				if (Ability->GetActivationPolicy() == EHollowAbilityActivationPolicy::WhileInputActive)
				{
					ToCancel.Add(Spec.Handle);
				}
			}
		}
	}
	for (const FGameplayAbilitySpecHandle& Handle : ToCancel)
	{
		CancelAbilityHandle(Handle);   // replicated to the server for predicted abilities
	}
}
