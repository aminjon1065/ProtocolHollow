#include "Characters/HollowCharacterBase.h"

#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "HollowGASLog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowCharacterBase)

AHollowCharacterBase::AHollowCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;   // CLAUDE.md sec. 1: event-driven, no Tick
}

UAbilitySystemComponent* AHollowCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent.Get();
}

void AHollowCharacterBase::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	if (const UHollowAbilitySystemComponent* AbilitySystem = AbilitySystemComponent.Get())
	{
		AbilitySystem->GetOwnedGameplayTags(TagContainer);
	}
}

void AHollowCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UninitializeAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

void AHollowCharacterBase::InitializeAbilitySystem(UHollowAbilitySystemComponent* InAbilitySystem, AActor* InOwnerActor)
{
	if (!InAbilitySystem || !InOwnerActor)
	{
		UE_LOG(LogHollowGAS, Error, TEXT("%s: cannot initialize the ability system without a component and an owner."), *GetName());
		return;
	}

	if (AbilitySystemComponent.Get() != InAbilitySystem)
	{
		UninitializeAbilitySystem();
	}
	AbilitySystemComponent = InAbilitySystem;
	InAbilitySystem->InitAbilityActorInfo(InOwnerActor, this);
}

void AHollowCharacterBase::UninitializeAbilitySystem()
{
	UHollowAbilitySystemComponent* AbilitySystem = AbilitySystemComponent.Get();
	AbilitySystemComponent.Reset();
	if (!AbilitySystem || AbilitySystem->GetAvatarActor() != this)
	{
		return;   // another avatar already took over
	}

	AbilitySystem->CancelAbilities();
	AbilitySystem->RemoveAllGameplayCues();
	if (AbilitySystem->GetOwnerActor())
	{
		AbilitySystem->SetAvatarActor(nullptr);
	}
	else
	{
		AbilitySystem->ClearActorInfo();
	}
}
