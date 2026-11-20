#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "GameplayTagAssetInterface.h"

#include "HollowCharacterBase.generated.h"

class UHollowAbilitySystemComponent;

/**
 * Shared base of players (ExoFrame) and enemies: ability system access and owned gameplay tags.
 * The ability system lives elsewhere (player state for players, the enemy itself for AI); derived classes bind it with
 * InitializeAbilitySystem. Movement components are chosen by derived classes (layering, ADR-0001).
 */
UCLASS(Abstract)
class HOLLOWGAS_API AHollowCharacterBase : public ACharacter, public IAbilitySystemInterface, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnHollowAbilitySystemChanged, UHollowAbilitySystemComponent* /*AbilitySystem (null: released)*/);

	explicit AHollowCharacterBase(const FObjectInitializer& ObjectInitializer);

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UHollowAbilitySystemComponent* GetHollowAbilitySystemComponent() const { return AbilitySystemComponent.Get(); }

	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;

	/**
	 * After this character became the avatar of an ability system, and before it releases it. On clients the ability
	 * system arrives with the player state, after possession: UI and other local listeners wait for this.
	 */
	FOnHollowAbilitySystemChanged OnAbilitySystemChanged;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Makes this character the avatar of an ability system owned by InOwnerActor (player state or itself). */
	void InitializeAbilitySystem(UHollowAbilitySystemComponent* InAbilitySystem, AActor* InOwnerActor);

	/** Releases the ability system if this character is still its avatar. */
	void UninitializeAbilitySystem();

private:
	TWeakObjectPtr<UHollowAbilitySystemComponent> AbilitySystemComponent;
};
