#include "Effects/HollowCarryEffects.h"

#include "GameplayEffectComponents/BlockAbilityTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "Tags/HollowGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowCarryEffects)

UHollowCarryEffect::UHollowCarryEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
}

void UHollowCarryEffect::InitCarryStructure(const FGameplayTag& StateTag)
{
	UTargetTagsGameplayEffectComponent* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("CarryingTag"));
	FInheritedTagContainer Granted;
	Granted.Added.AddTag(StateTag);
	TargetTags->SetAndApplyTargetTagChanges(Granted);
	GEComponents.Add(TargetTags);

	UBlockAbilityTagsGameplayEffectComponent* BlockedAbilities = CreateDefaultSubobject<UBlockAbilityTagsGameplayEffectComponent>(TEXT("NoTools"));
	FInheritedTagContainer Blocked;
	Blocked.Added.AddTag(HollowTags::Ability_Tool);
	BlockedAbilities->SetAndApplyBlockedAbilityTagChanges(Blocked);
	GEComponents.Add(BlockedAbilities);
}

UHollowCarrySoloEffect::UHollowCarrySoloEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InitCarryStructure(HollowTags::State_CarryingHeavy_Solo);
}

UHollowCarryCoopEffect::UHollowCarryCoopEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InitCarryStructure(HollowTags::State_CarryingHeavy_Coop);
}

UHollowStaminaRecoveryEffect::UHollowStaminaRecoveryEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	UTargetTagRequirementsGameplayEffectComponent* Requirements = CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(TEXT("NotWhileCarrying"));
	Requirements->OngoingTagRequirements.IgnoreTags.AddTag(HollowTags::State_CarryingHeavy);
	GEComponents.Add(Requirements);
}
