#include "Effects/HollowToolOverheatEffect.h"

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "Tags/HollowGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowToolOverheatEffect)

UHollowToolOverheatEffect::UHollowToolOverheatEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	UTargetTagsGameplayEffectComponent* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("OverheatedTag"));
	FInheritedTagContainer Tags;
	Tags.Added.AddTag(HollowTags::State_Tool_Overheated);
	TargetTags->SetAndApplyTargetTagChanges(Tags);
	GEComponents.Add(TargetTags);
}
