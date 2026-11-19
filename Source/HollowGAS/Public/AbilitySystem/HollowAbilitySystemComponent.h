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

private:
	void TryActivateAbilitiesOnSpawn();
};
