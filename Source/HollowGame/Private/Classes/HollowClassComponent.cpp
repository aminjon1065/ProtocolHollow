#include "Classes/HollowClassComponent.h"

#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Classes/HollowClassDefinition.h"
#include "Framework/HollowPlayerState.h"
#include "GameplayEffect.h"
#include "HollowGameLog.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowClassComponent)

UHollowClassComponent::UHollowClassComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

UHollowClassComponent* UHollowClassComponent::Get(const APlayerState* PlayerState)
{
	return PlayerState ? PlayerState->FindComponentByClass<UHollowClassComponent>() : nullptr;
}

void UHollowClassComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UHollowClassComponent, ClassId, Params);
}

const UHollowClassDefinition* UHollowClassComponent::GetDefinition() const
{
	if (!Definition || Definition->GetPrimaryAssetId() != ClassId)
	{
		Definition = UHollowClassDefinition::Load(ClassId);
	}
	return Definition;
}

UHollowAbilitySystemComponent* UHollowClassComponent::GetAbilitySystem() const
{
	const AHollowPlayerState* PlayerState = Cast<AHollowPlayerState>(GetOwner());
	return PlayerState ? PlayerState->GetHollowAbilitySystemComponent() : nullptr;
}

bool UHollowClassComponent::SetClass(const FPrimaryAssetId& Class)
{
	const UHollowClassDefinition* NewDefinition = UHollowClassDefinition::Load(Class);
	if (!GetOwner()->HasAuthority() || !NewDefinition)
	{
		UE_LOG(LogHollowGame, Warning, TEXT("%s: class %s refused (not on the server, or no such class)"), *GetNameSafe(GetOwner()), *Class.ToString());
		return false;
	}
	if (Class == ClassId)
	{
		return true;
	}
	Revoke();
	ClassId = Class;
	Definition = NewDefinition;
	MARK_PROPERTY_DIRTY_FROM_NAME(UHollowClassComponent, ClassId, this);
	Grant(*NewDefinition);
	UE_LOG(LogHollowGame, Log, TEXT("%s plays %s"), *GetNameSafe(GetOwner()), *Describe());
	OnClassChanged.Broadcast(ClassId);
	return true;
}

void UHollowClassComponent::Grant(const UHollowClassDefinition& ClassDefinition)
{
	UHollowAbilitySystemComponent* AbilitySystem = GetAbilitySystem();
	if (!AbilitySystem)
	{
		UE_LOG(LogHollowGame, Warning, TEXT("%s: no ability system for the class %s"), *GetNameSafe(GetOwner()), *ClassId.ToString());
		return;
	}
	// Tiny data assets, loaded once per class change on the server.
	if (const UHollowAbilitySet* AbilitySet = ClassDefinition.AbilitySet.LoadSynchronous())
	{
		AbilitySet->GiveToAbilitySystem(AbilitySystem, &GrantedAbilities, const_cast<UHollowClassDefinition*>(&ClassDefinition));
	}
	if (const TSubclassOf<UGameplayEffect> Effect = ClassDefinition.AttributeEffect.LoadSynchronous())
	{
		GrantedEffect = AbilitySystem->ApplyGameplayEffectToSelf(Effect->GetDefaultObject<UGameplayEffect>(), HollowAbilitySetDefaults::Level,
			AbilitySystem->MakeEffectContext());
	}
	if (ClassDefinition.ClassTag.IsValid())
	{
		// Loose, replicated to every machine (abilities and AI may ask anywhere).
		GrantedTag = ClassDefinition.ClassTag;
		AbilitySystem->AddLooseGameplayTag(GrantedTag, 1, EGameplayTagReplicationState::TagOnly);
	}
}

void UHollowClassComponent::Revoke()
{
	UHollowAbilitySystemComponent* AbilitySystem = GetAbilitySystem();
	if (!AbilitySystem)
	{
		return;
	}
	GrantedAbilities.TakeFromAbilitySystem(AbilitySystem);
	if (GrantedEffect.IsValid())
	{
		AbilitySystem->RemoveActiveGameplayEffect(GrantedEffect);
		GrantedEffect.Invalidate();
	}
	if (GrantedTag.IsValid())
	{
		AbilitySystem->RemoveLooseGameplayTag(GrantedTag, 1, EGameplayTagReplicationState::TagOnly);
		GrantedTag = FGameplayTag();
	}
}

void UHollowClassComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		Revoke();
	}
	Super::EndPlay(EndPlayReason);
}

void UHollowClassComponent::OnRep_ClassId()
{
	UE_LOG(LogHollowGame, Log, TEXT("%s plays %s"), *GetNameSafe(GetOwner()), *Describe());
	OnClassChanged.Broadcast(ClassId);
}

FString UHollowClassComponent::Describe() const
{
	const UHollowClassDefinition* ClassDefinition = GetDefinition();
	return FString::Printf(TEXT("%s (%s)"), ClassId.IsValid() ? *ClassId.PrimaryAssetName.ToString() : TEXT("no class"),
		ClassDefinition ? *ClassDefinition->ClassTag.ToString() : TEXT("-"));
}
