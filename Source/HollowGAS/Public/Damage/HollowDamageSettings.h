#pragma once

#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"

#include "HollowDamageSettings.generated.h"

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

	/** Types without a rule are reduced by armor and never ignored. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Types", meta = (TitleProperty = "DamageType"))
	TArray<FHollowDamageTypeRule> DamageTypes;

	const FHollowDamageTypeRule* FindRule(const FGameplayTag& DamageType) const;
};
