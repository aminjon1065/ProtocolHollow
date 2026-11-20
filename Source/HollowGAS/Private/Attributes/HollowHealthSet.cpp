#include "Attributes/HollowHealthSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowHealthSet)

TConstArrayView<FHollowBoundedAttribute> UHollowHealthSet::GetBoundedAttributes() const
{
	static const FHollowBoundedAttribute Bounded[] =
	{
		{ GetHealthAttribute(), GetMaxHealthAttribute() },
	};
	return Bounded;
}

bool UHollowHealthSet::IsMetaAttribute(const FGameplayAttribute& Attribute) const
{
	return Attribute == GetIncomingDamageAttribute() || Attribute == GetIncomingHealingAttribute();
}

void UHollowHealthSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// Health is clamped by PreAttributeBaseChange, so the meta inputs can be applied as plain deltas.
	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float Damage = GetIncomingDamage();
		SetIncomingDamage(0.f);
		if (Damage > 0.f)
		{
			const float HealthBefore = GetHealth();
			SetHealth(HealthBefore - Damage);
			if (HealthBefore > 0.f && GetHealth() <= 0.f)
			{
				const FGameplayEffectContextHandle& Context = Data.EffectSpec.GetContext();
				OnOutOfHealth.Broadcast(Context.GetEffectCauser(), Context.GetOriginalInstigator(), Damage);
			}
		}
	}
	else if (Data.EvaluatedData.Attribute == GetIncomingHealingAttribute())
	{
		const float Healing = GetIncomingHealing();
		SetIncomingHealing(0.f);
		if (Healing > 0.f)
		{
			SetHealth(GetHealth() + Healing);
		}
	}
}

void UHollowHealthSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UHollowHealthSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UHollowHealthSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UHollowHealthSet, Armor, COND_None, REPNOTIFY_Always);
}

void UHollowHealthSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHollowHealthSet, Health, OldValue);
}

void UHollowHealthSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHollowHealthSet, MaxHealth, OldValue);
}

void UHollowHealthSet::OnRep_Armor(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHollowHealthSet, Armor, OldValue);
}
