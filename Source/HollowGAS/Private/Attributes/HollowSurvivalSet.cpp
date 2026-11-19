#include "Attributes/HollowSurvivalSet.h"

#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowSurvivalSet)

TConstArrayView<FHollowBoundedAttribute> UHollowSurvivalSet::GetBoundedAttributes() const
{
	static const FHollowBoundedAttribute Bounded[] =
	{
		{ GetStaminaAttribute(), GetMaxStaminaAttribute() },
		{ GetOxygenAttribute(), GetMaxOxygenAttribute() },
		{ GetThermalAttribute(), GetMaxThermalAttribute() },
	};
	return Bounded;
}

void UHollowSurvivalSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UHollowSurvivalSet, Stamina, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UHollowSurvivalSet, MaxStamina, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UHollowSurvivalSet, Oxygen, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UHollowSurvivalSet, MaxOxygen, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UHollowSurvivalSet, Thermal, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UHollowSurvivalSet, MaxThermal, COND_OwnerOnly, REPNOTIFY_Always);
}

void UHollowSurvivalSet::OnRep_Stamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHollowSurvivalSet, Stamina, OldValue);
}

void UHollowSurvivalSet::OnRep_MaxStamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHollowSurvivalSet, MaxStamina, OldValue);
}

void UHollowSurvivalSet::OnRep_Oxygen(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHollowSurvivalSet, Oxygen, OldValue);
}

void UHollowSurvivalSet::OnRep_MaxOxygen(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHollowSurvivalSet, MaxOxygen, OldValue);
}

void UHollowSurvivalSet::OnRep_Thermal(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHollowSurvivalSet, Thermal, OldValue);
}

void UHollowSurvivalSet::OnRep_MaxThermal(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHollowSurvivalSet, MaxThermal, OldValue);
}
