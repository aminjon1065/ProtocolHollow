#include "Damage/HollowDamageSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowDamageSettings)

const FHollowDamageTypeRule* UHollowDamageSettings::FindRule(const FGameplayTag& DamageType) const
{
	return DamageType.IsValid() ? DamageTypes.FindByPredicate([&DamageType](const FHollowDamageTypeRule& Rule) { return Rule.DamageType == DamageType; }) : nullptr;
}
