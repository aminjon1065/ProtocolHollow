#include "Damage/HollowDamageSettings.h"

#include "AbilitySystemComponent.h"
#include "Attributes/HollowHealthSet.h"
#include "Tags/HollowGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowDamageSettings)

const FHollowDamageTypeRule* UHollowDamageSettings::FindRule(const FGameplayTag& DamageType) const
{
	return DamageType.IsValid() ? DamageTypes.FindByPredicate([&DamageType](const FHollowDamageTypeRule& Rule) { return Rule.DamageType == DamageType; }) : nullptr;
}

FHollowDamageOutcome HollowDamageRules::Resolve(const float Base, const float Armor, const FGameplayTagContainer* TargetTags, const FGameplayTag& DamageType,
	const float Penetration, const float ExposedPenetration)
{
	const UHollowDamageSettings* Settings = GetDefault<UHollowDamageSettings>();
	const FHollowDamageTypeRule* Rule = Settings->FindRule(DamageType);
	FHollowDamageOutcome Outcome;
	// Immune by the type's rule, or to everything with the exact Hollow.State.Invulnerable (its children, such as the
	// stalker's .Kinetic, only count through their own rule).
	const bool bInvulnerable = TargetTags && TargetTags->HasTagExact(HollowTags::State_Invulnerable);
	Outcome.bImmune = bInvulnerable || (Rule && Rule->ImmunityTag.IsValid() && TargetTags && TargetTags->HasTag(Rule->ImmunityTag));
	// GP-01: a weapon's hit ignores a share of the armour, more of it while the target is exposed (an Aether lantern's light).
	Outcome.bExposed = TargetTags && TargetTags->HasTag(HollowTags::State_Exposed);
	const float Pierce = Penetration + (Outcome.bExposed ? ExposedPenetration : 0.f);
	Outcome.Damage = HollowDamageMath::Mitigate(Base, Armor, Settings->ArmorConstant, Rule ? Rule->bArmorApplies : true, Outcome.bImmune, Pierce);
	return Outcome;
}

EHollowHitKind HollowDamageRules::Preview(const UAbilitySystemComponent* Target, const FGameplayTag& DamageType, const float Base, const float Penetration,
	const float ExposedPenetration)
{
	if (!Target || !Target->HasAttributeSetForAttribute(UHollowHealthSet::GetHealthAttribute()))
	{
		return EHollowHitKind::None;
	}
	FGameplayTagContainer Tags;
	Target->GetOwnedGameplayTags(Tags);
	const float Armor = Target->GetNumericAttribute(UHollowHealthSet::GetArmorAttribute());
	const FHollowDamageOutcome Outcome = Resolve(Base, Armor, &Tags, DamageType, Penetration, ExposedPenetration);
	return HollowDamageMath::Classify(Base, Outcome, GetDefault<UHollowDamageSettings>()->ArmorMarkerShare);
}
