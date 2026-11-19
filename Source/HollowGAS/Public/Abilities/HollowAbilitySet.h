#pragma once

#include "Engine/DataAsset.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "ActiveGameplayEffectHandle.h"

#include "HollowAbilitySet.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UHollowGameplayAbility;

namespace HollowAbilitySetDefaults
{
	/** Gameplay ability system levels start at 1. */
	inline constexpr int32 Level = 1;
}

/** One ability granted by an ability set. */
USTRUCT(BlueprintType)
struct FHollowAbilitySetAbility
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
	TSubclassOf<UHollowGameplayAbility> Ability;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability", meta = (ClampMin = "1"))
	int32 AbilityLevel = HollowAbilitySetDefaults::Level;

	/** Input that drives the ability (stored as a dynamic spec tag; routed in S2-03). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability", meta = (Categories = "Hollow.Input"))
	FGameplayTag InputTag;
};

/** One gameplay effect applied by an ability set (e.g. attribute initialization). */
USTRUCT(BlueprintType)
struct FHollowAbilitySetEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	TSubclassOf<UGameplayEffect> GameplayEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect", meta = (ClampMin = "1"))
	float EffectLevel = HollowAbilitySetDefaults::Level;
};

/** What an ability set granted, so it can be taken back exactly (e.g. when the pawn is unpossessed). */
USTRUCT()
struct HOLLOWGAS_API FHollowAbilitySetHandles
{
	GENERATED_BODY()

	/** Removes everything this handle set granted. Server only; no-op elsewhere. */
	void TakeFromAbilitySystem(UAbilitySystemComponent* AbilitySystem);

	bool IsEmpty() const { return AbilityHandles.IsEmpty() && EffectHandles.IsEmpty(); }
	int32 NumAbilities() const { return AbilityHandles.Num(); }

private:
	friend class UHollowAbilitySet;

	TArray<FGameplayAbilitySpecHandle> AbilityHandles;
	TArray<FActiveGameplayEffectHandle> EffectHandles;
};

/**
 * Data asset of abilities and effects granted together (e.g. DA_AbilitySet_ExoFrame). Attribute sets are not granted
 * here: they are fixed subobjects of their owner (player state or enemy), see Docs/Architecture/02 sec. 2.4.
 */
UCLASS(BlueprintType, Const)
class HOLLOWGAS_API UHollowAbilitySet : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Grants the set. Server only (CLAUDE.md sec. 3): does nothing without authority. */
	void GiveToAbilitySystem(UAbilitySystemComponent* AbilitySystem, FHollowAbilitySetHandles* OutHandles, UObject* SourceObject = nullptr) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (TitleProperty = "Ability"))
	TArray<FHollowAbilitySetAbility> GrantedAbilities;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects", meta = (TitleProperty = "GameplayEffect"))
	TArray<FHollowAbilitySetEffect> GrantedEffects;
};
