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
	Blocked.Added.AddTag(HollowTags::Ability_Weapon);       // GP-01: both hands hold the core
	Blocked.Added.AddTag(HollowTags::Ability_Melee);
	Blocked.Added.AddTag(HollowTags::Ability_Hands_Swap);
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

UHollowCarryDroneEffect::UHollowCarryDroneEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	UTargetTagsGameplayEffectComponent* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("DroneCarryTag"));
	FInheritedTagContainer Granted;
	Granted.Added.AddTag(HollowTags::State_DroneCarry);
	TargetTags->SetAndApplyTargetTagChanges(Granted);
	GEComponents.Add(TargetTags);
}

UHollowStaminaRecoveryEffect::UHollowStaminaRecoveryEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	UTargetTagRequirementsGameplayEffectComponent* Requirements = CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(TEXT("NotWhileCarrying"));
	Requirements->OngoingTagRequirements.IgnoreTags.AddTag(HollowTags::State_CarryingHeavy);
	GEComponents.Add(Requirements);
}
