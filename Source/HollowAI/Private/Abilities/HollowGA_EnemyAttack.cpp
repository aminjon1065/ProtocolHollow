#include "Abilities/HollowGA_EnemyAttack.h"

#include "Abilities/HollowEnemyAttackDefinition.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AIController.h"
#include "Attributes/HollowHealthSet.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HollowAILog.h"
#include "Tags/HollowGameplayTags.h"
#include "Threat/HollowAIPlayers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowGA_EnemyAttack)

UHollowGA_EnemyAttack::UHollowGA_EnemyAttack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;   // enemies live on the server
	ActivationBlockedTags.AddTag(HollowTags::State_Dead);
	ActivationBlockedTags.AddTag(HollowTags::State_Stunned);
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

UHollowGA_EnemyMelee::UHollowGA_EnemyMelee(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UHollowGA_EnemyMelee::CanStrike(const AActor* Target) const
{
	return HollowAIPlayers::IsLivingPlayer(Target);
}

void UHollowGA_EnemyMelee::Strike(AActor* Target)
{
	APawn* Attacker = GetAttacker();
	UAbilitySystemComponent* Source = GetAbilitySystemComponentFromActorInfo();
	if (!Attacker || !Source || !Attack->DamageEffect)
	{
		return;
	}
	UWorld* World = GetWorld();
	const FVector Forward = Attacker->GetActorForwardVector();
	const FVector Start = Attacker->GetActorLocation();
	const FVector End = Start + Forward * (Attacker->GetSimpleCollisionRadius() + Attack->Reach);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HollowEnemyMelee), /*bTraceComplex=*/false, Attacker);
	TArray<FHitResult> Hits;
	World->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Attack->Radius), Params);

	TArray<const APawn*> Struck;
	for (const FHitResult& Hit : Hits)
	{
		APawn* Victim = Cast<APawn>(Hit.GetActor());
		if (!HollowAIPlayers::IsLivingPlayer(Victim) || Struck.Contains(Victim) || FVector::DotProduct(Victim->GetActorLocation() - Start, Forward) <= 0.0)
		{
			continue;
		}
		FCollisionQueryParams Sight(SCENE_QUERY_STAT(HollowEnemyMeleeSight), /*bTraceComplex=*/false, Attacker);
		Sight.AddIgnoredActor(Victim);
		UAbilitySystemComponent* VictimAbilities = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Victim);
		if (!VictimAbilities || World->LineTraceTestByChannel(Attacker->GetPawnViewLocation(), Victim->GetActorLocation(), ECC_Visibility, Sight))
		{
			continue;   // behind cover
		}
		Struck.Add(Victim);
		const FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(Attack->DamageEffect, GetAbilityLevel());
		Spec.Data->SetSetByCallerMagnitude(HollowTags::Data_Damage, Attack->Damage);
		Source->ApplyGameplayEffectSpecToTarget(*Spec.Data, VictimAbilities);
		UE_LOG(LogHollowAI, Log, TEXT("%s: melee strikes %s (health %.0f)"), *Attacker->GetName(), *Victim->GetName(),
			VictimAbilities->GetNumericAttribute(UHollowHealthSet::GetHealthAttribute()));
	}
	UE_CLOG(Struck.IsEmpty(), LogHollowAI, Log, TEXT("%s: melee misses %s"), *Attacker->GetName(), *GetNameSafe(Target));
}
