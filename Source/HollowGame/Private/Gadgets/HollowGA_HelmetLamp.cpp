#include "Gadgets/HollowGA_HelmetLamp.h"

#include "Character/HollowExoFrameCharacter.h"
#include "Tags/HollowGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowGA_HelmetLamp)

UHollowGA_HelmetLamp::UHollowGA_HelmetLamp(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EHollowAbilityActivationPolicy::OnInputTriggered;
	SetAssetTags(FGameplayTagContainer(HollowTags::Ability_HelmetLamp));
	ActivationBlockedTags.AddTag(HollowTags::State_Dead);
}

bool UHollowGA_HelmetLamp::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	const AHollowExoFrameCharacter* Character = Cast<AHollowExoFrameCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	return Character && !Character->IsDead() && Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UHollowGA_HelmetLamp::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))   // the cooldown, predicted
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}
	if (AHollowExoFrameCharacter* Character = Cast<AHollowExoFrameCharacter>(GetAvatarActorFromActorInfo()))
	{
		if (HasAuthority(&ActivationInfo))
		{
			Character->SetHelmetLampOn(!Character->IsHelmetLampOn());
		}
		else if (FPredictionKey PredictionKey = ActivationInfo.GetActivationPredictionKey();
			ActivationInfo.ActivationMode == EGameplayAbilityActivationMode::Predicting && PredictionKey.IsValidKey())
		{
			PredictSwitch(*Character, PredictionKey.NewRejectedDelegate());
		}
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}

void UHollowGA_HelmetLamp::PredictSwitch(AHollowExoFrameCharacter& Character, FPredictionKeyEvent& OnRejected)
{
	Character.SetHelmetLampOn(!Character.IsHelmetLampOn());
	OnRejected.BindWeakLambda(&Character, [&Character]
	{
		Character.SetHelmetLampOn(!Character.IsHelmetLampOn());   // the server did not switch: undo ours
	});
}
