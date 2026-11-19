#include "Attributes/HollowToolSet.h"

#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowToolSet)

TConstArrayView<FHollowBoundedAttribute> UHollowToolSet::GetBoundedAttributes() const
{
	static const FHollowBoundedAttribute Bounded[] =
	{
		{ GetToolFuelAttribute(), GetMaxToolFuelAttribute() },
		{ GetToolHeatAttribute(), GetMaxToolHeatAttribute() },
	};
	return Bounded;
}

void UHollowToolSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UHollowToolSet, ToolFuel, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UHollowToolSet, MaxToolFuel, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UHollowToolSet, ToolHeat, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UHollowToolSet, MaxToolHeat, COND_OwnerOnly, REPNOTIFY_Always);
}

void UHollowToolSet::OnRep_ToolFuel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHollowToolSet, ToolFuel, OldValue);
}

void UHollowToolSet::OnRep_MaxToolFuel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHollowToolSet, MaxToolFuel, OldValue);
}

void UHollowToolSet::OnRep_ToolHeat(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHollowToolSet, ToolHeat, OldValue);
}

void UHollowToolSet::OnRep_MaxToolHeat(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHollowToolSet, MaxToolHeat, OldValue);
}
