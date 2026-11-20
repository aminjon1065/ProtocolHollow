#pragma once

#include "Abilities/HollowAbilitySet.h"
#include "Characters/HollowCharacterBase.h"

#include "HollowEnemyCharacter.generated.h"

class UHollowAbilitySystemComponent;
class UHollowEnemyDefinition;
class UHollowFootstepAudioComponent;
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

	/** In a vent shaft (ADR-0019): hidden, no collision, no movement, on every machine. Server sets it. */
	bool IsInVent() const { return bInVent; }
	void SetInVent(bool bInside);
	/** Server: it has just come out of a vent without going in (the director's spawn): the grille, here and wherever it appears. */
	void EmergeFromVent();

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

	/** Its footsteps for players (every machine), UHollowEnemyDefinition::FootstepSound. No AI hears enemies. */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Enemy")
	TObjectPtr<UHollowFootstepAudioComponent> FootstepAudio;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Enemy")
	TObjectPtr<UHollowEnemyDefinition> Definition;

private:
	void HandleOutOfHealth(AActor* EffectCauser, AActor* EffectInstigator, float Damage);
	void ApplyDeadPresentation();
	void ApplyVentPresentation();
	void PlayVentSound() const;
	/** Server: stamps the exit (its first bunch tells late machines) and rattles the grille. */
	void MarkVentExit();

	UFUNCTION()
	void OnRep_Dead();

	UFUNCTION()
	void OnRep_InVent();

	UPROPERTY(ReplicatedUsing = OnRep_Dead)
	bool bDead = false;

	UPROPERTY(ReplicatedUsing = OnRep_InVent)
	bool bInVent = false;

	/**
	 * Server time it last came out of a vent. A stalker in a vent is relevant to no one, so clients get a new actor when
	 * it comes out: its first bunch carries this, and a fresh one plays the grille there (S4-04).
	 */
	UPROPERTY(Replicated)
	float VentExitServerTime = 0.f;

	FHollowAbilitySetHandles GrantedAbilities;
	FDelegateHandle OutOfHealthHandle;
};
