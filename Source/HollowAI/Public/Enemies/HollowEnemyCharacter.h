#pragma once

#include "Abilities/HollowAbilitySet.h"
#include "Characters/HollowCharacterBase.h"
#include "Interfaces/HollowImpulseReceiver.h"
#include "Interfaces/HollowSeededActor.h"

#include "HollowEnemyCharacter.generated.h"

class UHollowAbilitySystemComponent;
class UHollowEnemyDefinition;
class UHollowEnemyMontageComponent;
class UHollowFootstepAudioComponent;
class UHollowHealthSet;
class UHollowMovementSet;
class AHollowEnemyCharacter;

/** Server: an enemy has just died (GP-02: the game drops its loot, UHollowEnemyDropSubsystem). */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnHollowEnemyDied, AHollowEnemyCharacter& /*Enemy*/);

/**
 * Enemy pawn (Docs/Architecture/02 sec. 2.7, ADR-0017): its own ability system in Minimal replication (tags and cues
 * reach clients, effects stay on the server) with a health set and a movement set (the speed multiplier effects slow,
 * S6-02), everything else from UHollowEnemyDefinition. Driven only on the server by AHollowAIController; clients see
 * replicated movement and the replicated dead flag.
 * The Iron Pilgrim is BP_IronPilgrim: this class plus data.
 */
UCLASS()
class HOLLOWAI_API AHollowEnemyCharacter : public AHollowCharacterBase, public IHollowImpulseReceiver, public IHollowSeededActor
{
	GENERATED_BODY()

public:
	explicit AHollowEnemyCharacter(const FObjectInitializer& ObjectInitializer);

	const UHollowEnemyDefinition* GetDefinition() const { return Definition; }
	UHollowHealthSet* GetHealthSet() const { return HealthSet; }
	UHollowEnemyMontageComponent* GetMontages() const { return Montages; }
	bool IsDead() const { return bDead; }

	/** Server: stops the brain and abilities, freezes in place and goes away after the definition's linger time. */
	void Die();

	/** Every enemy's death on the server, in any world (listeners check the enemy's world). */
	static FOnHollowEnemyDied& OnAnyEnemyDied();

	/**
	 * Server, before FinishSpawning or right after the spawn (GP-02): its seed, from the sector's (a socket's enemy) or
	 * its nest's: what it rolls of its own (its drop) is the same for the same sector.
	 */
	virtual void SetSpawnSeed(int32 InSeed) override { SpawnSeed = InSeed; }
	int32 GetSpawnSeed() const { return SpawnSeed; }

	/**
	 * Server (GP-01): a shot or a blow pushes it back by its definition's KnockbackScale, and a push stronger than
	 * StaggerImpulse staggers it (StaggerEffect). The movement replicates as always; a dead enemy or one in a vent stays.
	 */
	virtual void ReceiveImpulse(const FHollowImpulse& Impulse) override;

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

	/** It is dead on this machine (the server's Die, or the replicated flag): a child's own presentation. */
	virtual void PresentDeath() {}

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Enemy")
	TObjectPtr<UHollowAbilitySystemComponent> AbilitySystem;

	UPROPERTY()
	TObjectPtr<UHollowHealthSet> HealthSet;

	UPROPERTY()
	TObjectPtr<UHollowMovementSet> MovementSet;

	/** Its footsteps for players (every machine), UHollowEnemyDefinition::FootstepSound. No AI hears enemies. */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Enemy")
	TObjectPtr<UHollowFootstepAudioComponent> FootstepAudio;

	/** Its montages (UHollowEnemyDefinition::Montages, ADR-0027) on every machine; empty slots play nothing. */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Enemy")
	TObjectPtr<UHollowEnemyMontageComponent> Montages;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Enemy")
	TObjectPtr<UHollowEnemyDefinition> Definition;

private:
	void HandleOutOfHealth(AActor* EffectCauser, AActor* EffectInstigator, float Damage);
	void ApplyDeadPresentation();
	/**
	 * Collision and movement follow the vent state at once; going in (bAnimate: seen happen, not from a first bunch) the
	 * body hides once its VentEnter montage has played, which keeps it relevant meanwhile (ADR-0027).
	 */
	void ApplyVentPresentation(bool bAnimate);
	void PlayVentSound() const;
	/** The grille and the VentExit montage where it comes out. */
	void PresentVentExit();
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
	/** Server only (SetSpawnSeed). */
	int32 SpawnSeed = 0;
	FDelegateHandle OutOfHealthHandle;
	FTimerHandle VentHideTimer;
};
