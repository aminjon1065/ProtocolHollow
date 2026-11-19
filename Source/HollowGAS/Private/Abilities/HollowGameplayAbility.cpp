#include "Abilities/HollowGameplayAbility.h"

#include "AbilitySystemComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowGameplayAbility)

UHollowGameplayAbility::UHollowGameplayAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UHollowGameplayAbility::TryActivateOnSpawn(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) const
{
	if (ActivationPolicy != EHollowAbilityActivationPolicy::OnSpawn || !ActorInfo || Spec.IsActive())
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get();
	const AActor* Avatar = ActorInfo->AvatarActor.Get();
	if (!AbilitySystem || !Avatar || Avatar->GetTearOff())
	{
		return;
	}

	// Locally executed abilities start on the owning client; server-run abilities start on the authority.
	const bool bLocalExecution = NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::LocalPredicted
		|| NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::LocalOnly;
	const bool bServerExecution = NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::ServerOnly
		|| NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	if ((bLocalExecution && ActorInfo->IsLocallyControlled()) || (bServerExecution && ActorInfo->IsNetAuthority()))
	{
		AbilitySystem->TryActivateAbility(Spec.Handle);
	}
}

void UHollowGameplayAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	// The policy is a class default (EditDefaultsOnly): decide from the spec's class defaults, exactly like
	// UHollowAbilitySystemComponent does when a new avatar arrives, whether or not this call runs on an instance.
	if (const UHollowGameplayAbility* AbilityDefaults = Cast<UHollowGameplayAbility>(Spec.Ability))
	{
		AbilityDefaults->TryActivateOnSpawn(ActorInfo, Spec);
	}
}
