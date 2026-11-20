#include "Effects/HollowDeadEffect.h"

#include "GameplayEffectComponents/BlockAbilityTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "Tags/HollowGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowDeadEffect)

UHollowDeadEffect::UHollowDeadEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	UTargetTagsGameplayEffectComponent* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("DeadTag"));
	FInheritedTagContainer Granted;
	Granted.Added.AddTag(HollowTags::State_Dead);
	TargetTags->SetAndApplyTargetTagChanges(Granted);
	GEComponents.Add(TargetTags);

	UBlockAbilityTagsGameplayEffectComponent* BlockedAbilities = CreateDefaultSubobject<UBlockAbilityTagsGameplayEffectComponent>(TEXT("NoAbilities"));
	FInheritedTagContainer Blocked;
	Blocked.Added.AddTag(HollowTags::Ability_Tool);
	Blocked.Added.AddTag(HollowTags::Ability_Gadget_PulseMine);
	BlockedAbilities->SetAndApplyBlockedAbilityTagChanges(Blocked);
	GEComponents.Add(BlockedAbilities);
}
