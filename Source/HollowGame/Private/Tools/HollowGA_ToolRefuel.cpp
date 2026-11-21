#include "Tools/HollowGA_ToolRefuel.h"

#include "AbilitySystemComponent.h"
#include "Attributes/HollowToolSet.h"
#include "GameplayEffect.h"
#include "HollowGameLog.h"
#include "Tags/HollowGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowGA_ToolRefuel)

UHollowGA_ToolRefuel::UHollowGA_ToolRefuel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EHollowAbilityActivationPolicy::OnInputTriggered;
	ItemCostPerUse = 1;   // one fuel cell (DA_Item_FuelCell grants this)
	SetAssetTags(FGameplayTagContainer(HollowTags::Ability_Tool_Refuel));
	ActivationBlockedTags.AddTag(HollowTags::State_Dead);
}

bool UHollowGA_ToolRefuel::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	const UAbilitySystemComponent* AbilitySystem = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!RefuelEffect || !AbilitySystem)
	{
		return false;
	}
	const float Fuel = AbilitySystem->GetNumericAttribute(UHollowToolSet::GetToolFuelAttribute());
	const float MaxFuel = AbilitySystem->GetNumericAttribute(UHollowToolSet::GetMaxToolFuelAttribute());
	return Fuel < MaxFuel && Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UHollowGA_ToolRefuel::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}
	ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo, RefuelEffect->GetDefaultObject<UGameplayEffect>(), GetAbilityLevel(Handle, ActorInfo));
	UE_LOG(LogHollowGame, Log, TEXT("%s: refuels the tools (%.0f fuel)"), *GetNameSafe(GetAvatarActorFromActorInfo()),
		ActorInfo->AbilitySystemComponent->GetNumericAttribute(UHollowToolSet::GetToolFuelAttribute()));
	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}
