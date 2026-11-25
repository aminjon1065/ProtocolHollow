#include "Effects/HollowStatusEffects.h"

#include "GameplayEffectComponents/BlockAbilityTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "Tags/HollowGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowStatusEffects)

namespace HollowStatusEffectsPrivate
{
	/** A target tags component that grants Tag (the effect adds it to its components). */
	UTargetTagsGameplayEffectComponent& Grant(UTargetTagsGameplayEffectComponent& Component, const FGameplayTag& Tag)
	{
		FInheritedTagContainer Granted;
		Granted.Added.AddTag(Tag);
		Component.SetAndApplyTargetTagChanges(Granted);
		return Component;
	}
}

UHollowStunnedEffect::UHollowStunnedEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	GEComponents.Add(&HollowStatusEffectsPrivate::Grant(*CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("StunnedTag")), HollowTags::State_Stunned));

	UBlockAbilityTagsGameplayEffectComponent* Blocked = CreateDefaultSubobject<UBlockAbilityTagsGameplayEffectComponent>(TEXT("NoEnemyAbilities"));
	FInheritedTagContainer Abilities;
	Abilities.Added.AddTag(HollowTags::Ability_Enemy);
	Blocked->SetAndApplyBlockedAbilityTagChanges(Abilities);
	GEComponents.Add(Blocked);
}

UHollowWeaponFireCooldownEffect::UHollowWeaponFireCooldownEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	GEComponents.Add(&HollowStatusEffectsPrivate::Grant(*CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("CooldownTag")), HollowTags::Cooldown_Weapon_Fire));
}

UHollowMeleeCooldownEffect::UHollowMeleeCooldownEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	GEComponents.Add(&HollowStatusEffectsPrivate::Grant(*CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("CooldownTag")), HollowTags::Cooldown_Melee));
}

UHollowHandsSwapEffect::UHollowHandsSwapEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	GEComponents.Add(&HollowStatusEffectsPrivate::Grant(*CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("SwappingTag")), HollowTags::State_Hands_Swapping));
}

UHollowPulseMineCooldownEffect::UHollowPulseMineCooldownEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	GEComponents.Add(&HollowStatusEffectsPrivate::Grant(*CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("CooldownTag")), HollowTags::Cooldown_Gadget_PulseMine));
}

UHollowOccultWardCooldownEffect::UHollowOccultWardCooldownEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	GEComponents.Add(&HollowStatusEffectsPrivate::Grant(*CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("CooldownTag")), HollowTags::Cooldown_Gadget_OccultWard));
}

UHollowHelmetLampCooldownEffect::UHollowHelmetLampCooldownEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	GEComponents.Add(&HollowStatusEffectsPrivate::Grant(*CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("CooldownTag")), HollowTags::Cooldown_HelmetLamp));
}

UHollowToolRefuelCooldownEffect::UHollowToolRefuelCooldownEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	GEComponents.Add(&HollowStatusEffectsPrivate::Grant(*CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("CooldownTag")), HollowTags::Cooldown_Tool_Refuel));
}

UHollowStalkerTraitsEffect::UHollowStalkerTraitsEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	GEComponents.Add(&HollowStatusEffectsPrivate::Grant(*CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("KineticImmunity")), HollowTags::State_Invulnerable_Kinetic));

	// In the Aether lantern's light (S6-02) the traits are inhibited: no immunity until the light has gone.
	UTargetTagRequirementsGameplayEffectComponent* Light = CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(TEXT("NotInTheLight"));
	Light->OngoingTagRequirements.IgnoreTags.AddTag(HollowTags::State_Exposed);
	GEComponents.Add(Light);
}
