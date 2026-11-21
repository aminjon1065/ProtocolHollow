#pragma once

#include "Abilities/GameplayAbility.h"

#include "HollowGameplayAbility.generated.h"

/** How an ability gets activated. Input routing by Hollow.Input.* tags arrives with the first tools (S2-03). */
UENUM(BlueprintType)
enum class EHollowAbilityActivationPolicy : uint8
{
	/** Activated once when its input is pressed. */
	OnInputTriggered,
	/** Active while its input is held (continuous tools: cutter, welder). */
	WhileInputActive,
	/** Activated as soon as it is granted and an avatar exists (passives). */
	OnSpawn
};

/**
 * Base of every Hollow ability. Defaults follow CLAUDE.md sec. 4: one instance per actor and local prediction for
 * responsive first-person tools; authoritative validation stays on the server. An ability granted by an item (ADR-0030)
 * may cost that item per use (ItemCostPerUse).
 */
UCLASS()
class HOLLOWGAS_API UHollowGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	explicit UHollowGameplayAbility(const FObjectInitializer& ObjectInitializer);

	EHollowAbilityActivationPolicy GetActivationPolicy() const { return ActivationPolicy; }

	/** Activates the ability if its policy is OnSpawn, it is not active and this machine should run it. Called on class defaults. */
	void TryActivateOnSpawn(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) const;

	int32 GetItemCostPerUse() const { return ItemCostPerUse; }

	/**
	 * The item cost: with ItemCostPerUse above 0 the owner (IHollowItemCostSource) must hold that many of the spec's
	 * source object (the item that granted the ability). Checked on both sides, the owner's count replicating to it;
	 * a missing item adds the ability system's cost failure tag.
	 */
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	/** Takes the item cost on the server (the owning client learns it from the replicated count). */
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

protected:
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hollow|Ability")
	EHollowAbilityActivationPolicy ActivationPolicy = EHollowAbilityActivationPolicy::OnInputTriggered;

	/** Items of the granting item one use takes from the owner's raid inventory (a mine, a fuel cell); 0: free. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hollow|Cost", meta = (ClampMin = "0"))
	int32 ItemCostPerUse = 0;

	friend class FHollowAbilityOnSpawnTest;   // automation test switching the policy of the base class
};
