#include "Weapons/HollowGA_SwapHands.h"

#include "AbilitySystemComponent.h"
#include "Feel/HollowFeelSubsystem.h"
#include "Tags/HollowGameplayTags.h"
#include "Weapons/HollowHandsComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowGA_SwapHands)

UHollowGA_SwapHands::UHollowGA_SwapHands(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EHollowAbilityActivationPolicy::OnInputTriggered;
	SetAssetTags(FGameplayTagContainer(HollowTags::Ability_Hands_Swap));
	ActivationBlockedTags.AddTag(HollowTags::State_Dead);
	ActivationBlockedTags.AddTag(HollowTags::State_Weapon_Reloading);   // a reload is committed (its rounds are in)
	CancelAbilitiesWithTag.AddTag(HollowTags::Ability_Tool);
	CancelAbilitiesWithTag.AddTag(HollowTags::Ability_Weapon);
}

EHollowHandsRequest UHollowGA_SwapHands::GetRequest(const FGameplayAbilitySpec& Spec)
{
	const FGameplayTagContainer& Inputs = Spec.GetDynamicSpecSourceTags();
	if (Inputs.HasTagExact(HollowTags::Input_Hands_Tools))
	{
		return EHollowHandsRequest::Tools;
	}
	return Inputs.HasTagExact(HollowTags::Input_Hands_Weapon) ? EHollowHandsRequest::Weapon : EHollowHandsRequest::Cycle;
}

EHollowHandsMode UHollowGA_SwapHands::Target(const UHollowHandsComponent& Hands, const FGameplayAbilitySpec* Spec)
{
	return Spec ? Hands.Resolve(GetRequest(*Spec)) : Hands.GetMode();
}

bool UHollowGA_SwapHands::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	const UHollowHandsComponent* Hands = UHollowHandsComponent::Get(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	const UAbilitySystemComponent* AbilitySystem = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!Hands || !AbilitySystem || Target(*Hands, AbilitySystem->FindAbilitySpecFromHandle(Handle)) == Hands->GetMode())
	{
		return false;   // nothing to switch to (already there, or no weapon)
	}
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UHollowGA_SwapHands::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	UHollowHandsComponent* Hands = UHollowHandsComponent::Get(GetAvatarActorFromActorInfo());
	if (!Hands || !CommitAbility(Handle, ActorInfo, ActivationInfo))   // the hands' busy moment, predicted
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}
	const EHollowHandsMode Previous = Hands->GetMode();
	const EHollowHandsMode Wanted = Target(*Hands, GetCurrentAbilitySpec());
	if (UHollowFeelSubsystem* Feel = UHollowFeelSubsystem::Get(Hands); Feel && ActorInfo->IsLocallyControlled())
	{
		Feel->EmitFor(HollowTags::Feel_Swap, GetAvatarActorFromActorInfo());   // GP-01c: the view dips as the hands change
	}
	if (HasAuthority(&ActivationInfo))
	{
		Hands->SetMode(Wanted);
	}
	else if (FPredictionKey PredictionKey = ActivationInfo.GetActivationPredictionKey();
		ActivationInfo.ActivationMode == EGameplayAbilityActivationMode::Predicting && PredictionKey.IsValidKey())
	{
		Hands->SetMode(Wanted);
		PredictionKey.NewRejectedDelegate().BindWeakLambda(Hands, [Hands, Previous]
		{
			Hands->SetMode(Previous);   // the server did not switch: undo ours
		});
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}
