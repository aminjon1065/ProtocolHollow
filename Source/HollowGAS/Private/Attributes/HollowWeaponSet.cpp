#include "Attributes/HollowWeaponSet.h"

#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowWeaponSet)

TConstArrayView<FHollowBoundedAttribute> UHollowWeaponSet::GetBoundedAttributes() const
{
	static const FHollowBoundedAttribute Bounded[] =
	{
		{ GetMagazineAttribute(), GetMaxMagazineAttribute() },
	};
	return Bounded;
}

void UHollowWeaponSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UHollowWeaponSet, Magazine, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UHollowWeaponSet, MaxMagazine, COND_OwnerOnly, REPNOTIFY_Always);
}

void UHollowWeaponSet::OnRep_Magazine(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHollowWeaponSet, Magazine, OldValue);
}

void UHollowWeaponSet::OnRep_MaxMagazine(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHollowWeaponSet, MaxMagazine, OldValue);
}
