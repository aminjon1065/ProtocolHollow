#include "Effects/HollowOccultEffects.h"

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "Tags/HollowGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowOccultEffects)

namespace HollowOccultEffectsPrivate
{
	/** One instance per target: a renewal only restarts its duration. */
	constexpr int32 OneInstance = 1;
	/** Cue levels: the glow whatever the effect's level. */
	constexpr float AnyLevelMin = 0.f;
	constexpr float AnyLevelMax = 1.f;
}

UHollowLanternEffect::UHollowLanternEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	using namespace HollowOccultEffectsPrivate;
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	PRAGMA_DISABLE_DEPRECATION_WARNINGS   // UE 5.7 made StackingType private-to-be; its setter is editor only, the structure is native
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	StackLimitCount = OneInstance;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	StackExpirationPolicy = EGameplayEffectStackingExpirationPolicy::ClearEntireStack;
	// A renewal must not re-trigger the cues: every one would be a multicast RPC, ten a second per lit enemy.
	bSuppressStackingCues = true;
}

void UHollowLanternEffect::InitLanternStructure(const FGameplayTag& StateTag)
{
	UTargetTagsGameplayEffectComponent* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("StateTag"));
	FInheritedTagContainer Granted;
	Granted.Added.AddTag(StateTag);
	TargetTags->SetAndApplyTargetTagChanges(Granted);
	GEComponents.Add(TargetTags);
}

UHollowOccultExposeEffect::UHollowOccultExposeEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	using namespace HollowOccultEffectsPrivate;
	InitLanternStructure(HollowTags::State_Exposed);
	GameplayCues.Add(FGameplayEffectCue(HollowTags::GameplayCue_Status_Exposed, AnyLevelMin, AnyLevelMax));
}

UHollowAntiResonanceEffect::UHollowAntiResonanceEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InitLanternStructure(HollowTags::State_AntiResonance);
}
