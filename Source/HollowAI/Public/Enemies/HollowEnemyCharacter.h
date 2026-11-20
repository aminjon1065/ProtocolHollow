#pragma once

#include "Abilities/HollowAbilitySet.h"
#include "Characters/HollowCharacterBase.h"

#include "HollowEnemyCharacter.generated.h"

class UHollowAbilitySystemComponent;
class UHollowEnemyDefinition;
class UHollowHealthSet;

/**
 * Enemy pawn (Docs/Architecture/02 sec. 2.7, ADR-0017): its own ability system in Minimal replication (tags and cues
 * reach clients, effects stay on the server) with a health set, everything else from UHollowEnemyDefinition. Driven
 * only on the server by AHollowAIController; clients see replicated movement and the replicated dead flag.
 * The Iron Pilgrim is BP_IronPilgrim: this class plus data.
 */
UCLASS()
class HOLLOWAI_API AHollowEnemyCharacter : public AHollowCharacterBase
{
	GENERATED_BODY()

public:
	explicit AHollowEnemyCharacter(const FObjectInitializer& ObjectInitializer);

	const UHollowEnemyDefinition* GetDefinition() const { return Definition; }
	UHollowHealthSet* GetHealthSet() const { return HealthSet; }
	bool IsDead() const { return bDead; }

	/** Server: stops the brain and abilities, freezes in place and goes away after the definition's linger time. */
	void Die();

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Enemy")
	TObjectPtr<UHollowAbilitySystemComponent> AbilitySystem;

	UPROPERTY()
	TObjectPtr<UHollowHealthSet> HealthSet;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Enemy")
	TObjectPtr<UHollowEnemyDefinition> Definition;

private:
	void HandleOutOfHealth(AActor* EffectCauser, AActor* EffectInstigator, float Damage);
	void ApplyDeadPresentation();

	UFUNCTION()
	void OnRep_Dead();

	UPROPERTY(ReplicatedUsing = OnRep_Dead)
	bool bDead = false;

	FHollowAbilitySetHandles GrantedAbilities;
	FDelegateHandle OutOfHealthHandle;
};
