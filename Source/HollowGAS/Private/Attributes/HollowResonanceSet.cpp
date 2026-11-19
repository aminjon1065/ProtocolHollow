#include "Attributes/HollowResonanceSet.h"

#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowResonanceSet)

TConstArrayView<FHollowBoundedAttribute> UHollowResonanceSet::GetBoundedAttributes() const
{
	static const FHollowBoundedAttribute Bounded[] =
	{
		{ GetResonanceAttribute(), GetMaxResonanceAttribute() },
	};
	return Bounded;
}

void UHollowResonanceSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UHollowResonanceSet, Resonance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UHollowResonanceSet, MaxResonance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UHollowResonanceSet, ResonanceGainMultiplier, COND_None, REPNOTIFY_Always);
}

void UHollowResonanceSet::OnRep_Resonance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHollowResonanceSet, Resonance, OldValue);
}

void UHollowResonanceSet::OnRep_MaxResonance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHollowResonanceSet, MaxResonance, OldValue);
}

void UHollowResonanceSet::OnRep_ResonanceGainMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHollowResonanceSet, ResonanceGainMultiplier, OldValue);
}
