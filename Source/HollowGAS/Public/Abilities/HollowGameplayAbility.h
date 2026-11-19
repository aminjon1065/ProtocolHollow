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
 * responsive first-person tools; authoritative validation stays on the server.
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

protected:
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hollow|Ability")
	EHollowAbilityActivationPolicy ActivationPolicy = EHollowAbilityActivationPolicy::OnInputTriggered;

	friend class FHollowAbilityOnSpawnTest;   // automation test switching the policy of the base class
};
