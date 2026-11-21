#include "Enemies/HollowEnemyMontageComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Enemies/HollowEnemyDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Character.h"
#include "HollowAILog.h"
#include "Tags/HollowGameplayTags.h"
#include "Util/HollowEnumUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowEnemyMontageComponent)

UHollowEnemyMontageComponent::UHollowEnemyMontageComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UHollowEnemyMontageComponent* UHollowEnemyMontageComponent::Get(const AActor* Owner)
{
	return Owner ? Owner->FindComponentByClass<UHollowEnemyMontageComponent>() : nullptr;
}

void UHollowEnemyMontageComponent::SetDefinition(const UHollowEnemyDefinition* InDefinition)
{
	Definition = InDefinition;
}

void UHollowEnemyMontageComponent::BeginPlay()
{
	Super::BeginPlay();
	const TArray<FSoftObjectPath> Assigned = Definition ? Definition->Montages.Assigned() : TArray<FSoftObjectPath>();
	if (!Assigned.IsEmpty())
	{
		Loading = UAssetManager::GetStreamableManager().RequestAsyncLoad(Assigned);
	}
	// The stun reaches every machine as a tag of the enemy's ability system (Minimal replication).
	if (UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		StunSource = AbilitySystem;
		StunnedHandle = AbilitySystem->RegisterGameplayTagEvent(HollowTags::State_Stunned, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleStunned);
	}
}

void UHollowEnemyMontageComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAbilitySystemComponent* AbilitySystem = StunSource.Get())
	{
		AbilitySystem->RegisterGameplayTagEvent(HollowTags::State_Stunned, EGameplayTagEventType::NewOrRemoved).Remove(StunnedHandle);
	}
	StunSource.Reset();
	if (Loading.IsValid())
	{
		Loading->CancelHandle();
		Loading.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

float UHollowEnemyMontageComponent::Play(const EHollowEnemyMontage Moment, const float FitSeconds)
{
	if (!Definition)
	{
		return 0.f;
	}
	const TSoftObjectPtr<UAnimMontage>& Slot = Definition->Montages.Get(Moment);
	if (Slot.IsNull())
	{
		return 0.f;   // greybox: nothing to show, nothing to say
	}
	UAnimMontage* Montage = Slot.Get();
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	UAnimInstance* Animation = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (!Montage)
	{
		UE_LOG(LogHollowAI, Verbose, TEXT("%s: montage %s still loading"), *GetNameSafe(GetOwner()), *HollowEnumToString(Moment));
		return 0.f;
	}
	if (!Animation)
	{
		UE_CLOG(!bWarnedNoAnimation, LogHollowAI, Warning, TEXT("%s: has montages (%s) but its mesh has no animation instance with a slot to play them."),
			*GetNameSafe(GetOwner()), *Montage->GetName());
		bWarnedNoAnimation = true;
		return 0.f;
	}
	const float Rate = FitSeconds > 0.f ? HollowEnemyMontages::PlayRateToLand(*Montage, Definition->ImpactSection, FitSeconds) : 1.f;
	const float Length = Animation->Montage_Play(Montage, Rate);
	if (Length <= 0.f)
	{
		return 0.f;
	}
	LastPlayed = Moment;
	++Plays;
	UE_LOG(LogHollowAI, Verbose, TEXT("%s: montage %s (%s) at rate %.2f"), *GetNameSafe(GetOwner()), *HollowEnumToString(Moment), *Montage->GetName(), Rate);
	return Length / Rate;
}

void UHollowEnemyMontageComponent::Stop(const EHollowEnemyMontage Moment)
{
	const UAnimMontage* Montage = Definition ? Definition->Montages.Get(Moment).Get() : nullptr;
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	UAnimInstance* Animation = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (Montage && Animation && Animation->Montage_IsPlaying(Montage))
	{
		Animation->Montage_Stop(Montage->GetDefaultBlendOutTime(), Montage);
	}
}

void UHollowEnemyMontageComponent::HandleStunned(FGameplayTag Tag, const int32 Count)
{
	if (Count > 0)
	{
		Play(EHollowEnemyMontage::Stunned);
	}
	else
	{
		Stop(EHollowEnemyMontage::Stunned);
	}
}
