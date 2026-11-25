#pragma once

#include "Damage/HollowDamageMath.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"

#include "HollowDamageSettings.generated.h"

class UAbilitySystemComponent;

/** How one damage type meets its target (Docs/Architecture/02 sec. 2.4, ADR-0017). */
USTRUCT(BlueprintType)
struct FHollowDamageTypeRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (Categories = "Hollow.Damage"))
	FGameplayTag DamageType;

	/** Whether the target's Armor reduces it (occult damage goes straight through). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	bool bArmorApplies = true;

	/** A target owning this tag takes none of it (e.g. the Echo-Stalker and kinetic damage). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (Categories = "Hollow.State"))
	FGameplayTag ImmunityTag;
};

/** Damage tuning of UHollowDamageExecution, in DefaultGame.ini (no rebuild to retune). */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Hollow Damage"))
class HOLLOWGAS_API UHollowDamageSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Armor curve: damage x K / (K + Armor), so Armor = K halves the damage. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Armor", meta = (ClampMin = "0"))
	float ArmorConstant = 0.f;

	/** Whether the players' weapons and blows hurt each other (GP-01, ADR-0044); off: a pellet stops at a squad mate, harmless. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Players")
	bool bFriendlyFire = false;

	/** GP-01c (ADR-0046): the hit marker shows armour when it took at least this share of a hit. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Players", meta = (ClampMin = "0", ClampMax = "1"))
	float ArmorMarkerShare = 0.f;

	/** Types without a rule are reduced by armor and never ignored. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Types", meta = (TitleProperty = "DamageType"))
	TArray<FHollowDamageTypeRule> DamageTypes;

	const FHollowDamageTypeRule* FindRule(const FGameplayTag& DamageType) const;
};

/** Damage by the settings' rules: the one way the execution resolves a hit, and the shooter predicts its marker. */
namespace HollowDamageRules
{
	/**
	 * Base points of DamageType against a target with Armor and TargetTags: immune by the type's rule or by the exact
	 * Hollow.State.Invulnerable, exposed with Hollow.State.Exposed (then ExposedPenetration adds to Penetration).
	 */
	HOLLOWGAS_API FHollowDamageOutcome Resolve(float Base, float Armor, const FGameplayTagContainer* TargetTags, const FGameplayTag& DamageType,
		float Penetration, float ExposedPenetration);

	/**
	 * The hit marker a hit would show, from what this machine knows of Target (its replicated Armor and tags): the
	 * shooter's guess, no round trip (GP-01c). None without an ability system with health (a prop: the caller decides).
	 */
	HOLLOWGAS_API EHollowHitKind Preview(const UAbilitySystemComponent* Target, const FGameplayTag& DamageType, float Base, float Penetration,
		float ExposedPenetration);
}
