#include "Attributes/HollowGearSet.h"

#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowGearSet)

namespace HollowGearSetPrivate
{
	/** A multiplier that changes nothing: the player's gear without a relic. */
	constexpr float NeutralMultiplier = 1.f;
}

UHollowGearSet::UHollowGearSet()
{
	using namespace HollowGearSetPrivate;
	InitFootstepNoiseMultiplier(NeutralMultiplier);
	InitToolWorkMultiplier(NeutralMultiplier);
	InitCarrySpeedMultiplier(NeutralMultiplier);
}

float UHollowGearSet::GetMultiplier(const UAbilitySystemComponent* AbilitySystem, const FGameplayAttribute& Attribute)
{
	return AbilitySystem && AbilitySystem->HasAttributeSetForAttribute(Attribute) ? AbilitySystem->GetNumericAttribute(Attribute)
		: HollowGearSetPrivate::NeutralMultiplier;
}

void UHollowGearSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UHollowGearSet, FootstepNoiseMultiplier, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UHollowGearSet, ToolWorkMultiplier, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UHollowGearSet, CarrySpeedMultiplier, COND_OwnerOnly, REPNOTIFY_Always);
}

void UHollowGearSet::OnRep_FootstepNoiseMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHollowGearSet, FootstepNoiseMultiplier, OldValue);
}

void UHollowGearSet::OnRep_ToolWorkMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHollowGearSet, ToolWorkMultiplier, OldValue);
}

void UHollowGearSet::OnRep_CarrySpeedMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHollowGearSet, CarrySpeedMultiplier, OldValue);
}
