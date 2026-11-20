#include "Abilities/HollowGA_EnemyAttack.h"

#include "AbilitySystemComponent.h"
#include "Abilities/HollowEnemyAttackDefinition.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "Tags/HollowGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowGA_EnemyAttack)

UHollowGA_EnemyAttack::UHollowGA_EnemyAttack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;   // enemies live on the server
	ActivationBlockedTags.AddTag(HollowTags::State_Dead);
}

void UHollowGA_EnemyAttack::SetTrigger(const FGameplayTag& Event, const FGameplayTag& AbilityTag)
{
	TriggerEvent = Event;
	FAbilityTriggerData& Trigger = AbilityTriggers.AddDefaulted_GetRef();
	Trigger.TriggerTag = Event;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	SetAssetTags(FGameplayTagContainer(AbilityTag));
}

APawn* UHollowGA_EnemyAttack::GetAttacker() const
{
	return Cast<APawn>(GetAvatarActorFromActorInfo());
}

void UHollowGA_EnemyAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AActor* Target = TriggerEventData ? const_cast<AActor*>(TriggerEventData->Target.Get()) : nullptr;
	APawn* Attacker = GetAttacker();
	if (!Attack || !Attacker || !CanStrike(Target) || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/false, /*bWasCancelled=*/true);
		return;
	}
	CurrentTarget = Target;
	if (AAIController* Controller = Cast<AAIController>(Attacker->GetController()))
	{
		Controller->StopMovement();
		Controller->SetFocus(Target, EAIFocusPriority::Gameplay);
	}
	if (Attack->WindupCue.IsValid())
	{
		ActorInfo->AbilitySystemComponent->AddGameplayCue(Attack->WindupCue);
		bWindupCueActive = true;
	}
	UAbilityTask_WaitDelay* Windup = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(Attack->WindupSeconds, UE_KINDA_SMALL_NUMBER));
	Windup->OnFinish.AddDynamic(this, &ThisClass::HandleWindupFinished);
	Windup->ReadyForActivation();
}

void UHollowGA_EnemyAttack::HandleWindupFinished()
{
	RemoveWindupCue();
	if (AActor* Target = CurrentTarget.Get())
	{
		Strike(Target);
	}
	UAbilityTask_WaitDelay* Recovery = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(Attack->RecoverySeconds, UE_KINDA_SMALL_NUMBER));
	Recovery->OnFinish.AddDynamic(this, &ThisClass::HandleRecoveryFinished);
	Recovery->ReadyForActivation();
}

void UHollowGA_EnemyAttack::HandleRecoveryFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/false, /*bWasCancelled=*/false);
}

void UHollowGA_EnemyAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const bool bReplicateEndAbility, const bool bWasCancelled)
{
	RemoveWindupCue();
	if (const APawn* Attacker = GetAttacker())
	{
		if (AAIController* Controller = Cast<AAIController>(Attacker->GetController()))
		{
			Controller->ClearFocus(EAIFocusPriority::Gameplay);
		}
	}
	CurrentTarget.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UHollowGA_EnemyAttack::RemoveWindupCue()
{
	if (bWindupCueActive && Attack && CurrentActorInfo && CurrentActorInfo->AbilitySystemComponent.IsValid())
	{
		CurrentActorInfo->AbilitySystemComponent->RemoveGameplayCue(Attack->WindupCue);
	}
	bWindupCueActive = false;
}
