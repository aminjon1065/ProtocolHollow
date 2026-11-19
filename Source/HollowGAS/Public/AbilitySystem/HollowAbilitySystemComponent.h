#pragma once

#include "AbilitySystemComponent.h"

#include "HollowAbilitySystemComponent.generated.h"

/**
 * Ability system of Protocol: Hollow. Players own it on their player state (replication mode Mixed), enemies on
 * themselves (Minimal); the owner sets the mode. Re-activates OnSpawn abilities whenever a new avatar takes over.
 */
UCLASS()
class HOLLOWGAS_API UHollowAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;

	/**
	 * Input routing by Hollow.Input.* tag (UHollowAbilitySet puts the tag on the spec). Pressed activates
	 * OnInputTriggered and WhileInputActive abilities; released cancels WhileInputActive ones (continuous tools).
	 * Called on the owning client (or the listen-server host); LocalPredicted abilities replicate from there.
	 */
	void AbilityInputTagPressed(const FGameplayTag& InputTag);
	void AbilityInputTagReleased(const FGameplayTag& InputTag);

private:
	void TryActivateAbilitiesOnSpawn();
};
