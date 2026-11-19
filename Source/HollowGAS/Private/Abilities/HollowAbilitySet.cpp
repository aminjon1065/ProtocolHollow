#include "Abilities/HollowAbilitySet.h"

#include "Abilities/HollowGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "HollowGASLog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowAbilitySet)

void FHollowAbilitySetHandles::TakeFromAbilitySystem(UAbilitySystemComponent* AbilitySystem)
{
	if (!AbilitySystem || !AbilitySystem->IsOwnerActorAuthoritative())
	{
		return;
	}

	for (const FGameplayAbilitySpecHandle& Handle : AbilityHandles)
	{
		if (Handle.IsValid())
		{
			AbilitySystem->ClearAbility(Handle);
		}
	}
	for (const FActiveGameplayEffectHandle& Handle : EffectHandles)
	{
		if (Handle.IsValid())
		{
			AbilitySystem->RemoveActiveGameplayEffect(Handle);
		}
	}
	AbilityHandles.Reset();
	EffectHandles.Reset();
}

void UHollowAbilitySet::GiveToAbilitySystem(UAbilitySystemComponent* AbilitySystem, FHollowAbilitySetHandles* OutHandles,
	UObject* SourceObject) const
{
	if (!AbilitySystem || !AbilitySystem->IsOwnerActorAuthoritative())
	{
		return;
	}

	for (int32 Index = 0; Index < GrantedAbilities.Num(); ++Index)
	{
		const FHollowAbilitySetAbility& Entry = GrantedAbilities[Index];
		if (!IsValid(Entry.Ability))
		{
			UE_LOG(LogHollowGAS, Error, TEXT("%s: GrantedAbilities[%d] has no ability class."), *GetName(), Index);
			continue;
		}

		FGameplayAbilitySpec Spec(Entry.Ability, Entry.AbilityLevel, INDEX_NONE, SourceObject);
		if (Entry.InputTag.IsValid())
		{
			Spec.GetDynamicSpecSourceTags().AddTag(Entry.InputTag);
		}

		const FGameplayAbilitySpecHandle Handle = AbilitySystem->GiveAbility(Spec);
		if (OutHandles)
		{
			OutHandles->AbilityHandles.Add(Handle);
		}
	}

	for (int32 Index = 0; Index < GrantedEffects.Num(); ++Index)
	{
		const FHollowAbilitySetEffect& Entry = GrantedEffects[Index];
		if (!IsValid(Entry.GameplayEffect))
		{
			UE_LOG(LogHollowGAS, Error, TEXT("%s: GrantedEffects[%d] has no effect class."), *GetName(), Index);
			continue;
		}

		const UGameplayEffect* Effect = Entry.GameplayEffect->GetDefaultObject<UGameplayEffect>();
		const FActiveGameplayEffectHandle Handle = AbilitySystem->ApplyGameplayEffectToSelf(Effect, Entry.EffectLevel, AbilitySystem->MakeEffectContext());
		if (OutHandles && Handle.IsValid())   // instant effects leave nothing to remove
		{
			OutHandles->EffectHandles.Add(Handle);
		}
	}
}
