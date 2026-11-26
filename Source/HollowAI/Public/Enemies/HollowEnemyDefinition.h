#pragma once

#include "Audio/HollowFootstepAudioComponent.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "Enemies/HollowEnemyMontages.h"
#include "UObject/PrimaryAssetId.h"

#include "HollowEnemyDefinition.generated.h"

class UGameplayEffect;
class UHollowAbilitySet;
class UNavigationQueryFilter;
class UStateTree;

/**
 * What an enemy may leave when it dies (GP-02, ADR-0047): an item as a pickup, now and then. The AI only says so
 * (AHollowEnemyCharacter::OnAnyEnemyDied); the game spawns the pickup (UHollowEnemyDropSubsystem).
 */
USTRUCT(BlueprintType)
struct HOLLOWAI_API FHollowEnemyDrop
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drop", meta = (AllowedTypes = "HollowItem"))
	FPrimaryAssetId Item;

	/** How many of the item (0: never drops). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drop", meta = (ClampMin = "0"))
	int32 Count = 0;

	/** Share of deaths that drop it, rolled from the enemy's spawn seed (the same sector drops the same). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drop", meta = (ClampMin = "0", ClampMax = "1"))
	float Chance = 0.f;

	/** The pickup actor the game spawns (BP_LootPickup). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drop")
	TSoftClassPtr<AActor> PickupClass;

	/** How fast it pops out of the body (cm/s, up). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drop", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float PopSpeed = 0.f;

	bool IsSet() const { return Item.IsValid() && Count > 0 && Chance > 0.f && !PickupClass.IsNull(); }

	/** Whether a death with Seed drops it (deterministic: FRandomStream of the seed). */
	bool Rolls(int32 Seed) const;
};

/** Tuning of one enemy type, primary asset type HollowEnemy (Docs/Architecture/02 sec. 2.7, ADR-0017). */
UCLASS(BlueprintType)
class HOLLOWAI_API UHollowEnemyDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	/** Whether it breaks what bars its way (BreachEvent required); one that cannot only opens doors and goes around. */
	virtual bool CanBreach() const { return true; }

	/** Attacks and the attribute initialization, granted on the server when the enemy is possessed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Abilities")
	TObjectPtr<UHollowAbilitySet> AbilitySet;

	/** Brain: which activity when (StateTree AI component schema). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Brain")
	TObjectPtr<UStateTree> StateTree;

	/** Filter of every path it plans (NAV_Filter_Heavy: welded doors and bulkheads cost, never block). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	TSubclassOf<UNavigationQueryFilter> NavigationFilter;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float PatrolSpeed = 0.f;

	/** Speed while chasing and investigating. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float ChaseSpeed = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0", Units = "DegreesPerSecond"))
	float TurnRate = 0.f;

	/** How close to a patrol point or a stimulus counts as there. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0", Units = "Centimeters"))
	float MoveAcceptanceRadius = 0.f;

	/** Replication rate of the enemy (it is server-driven; clients interpolate). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "1", Units = "Hertz"))
	float NetUpdateFrequency = 0.f;

	/** Patrol points are random reachable points this far around where it spawned. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Patrol", meta = (ClampMin = "0", Units = "Centimeters"))
	float PatrolRadius = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Patrol", meta = (ClampMin = "0.01", Units = "Seconds"))
	float PatrolPauseSeconds = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Senses", meta = (ClampMin = "0", Units = "Centimeters"))
	float SightRadius = 0.f;

	/** A seen target stays seen up to this distance (larger than SightRadius). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Senses", meta = (ClampMin = "0", Units = "Centimeters"))
	float LoseSightRadius = 0.f;

	/** Half of the field of view. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Senses", meta = (ClampMin = "0", ClampMax = "180", Units = "Degrees"))
	float SightHalfAngle = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Senses", meta = (ClampMin = "0", Units = "Centimeters"))
	float HearingRange = 0.f;

	/** Perception forgets a stimulus after this long (0: never). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Senses", meta = (ClampMin = "0", Units = "Seconds"))
	float StimulusMaxAge = 0.f;

	/** A target out of sight stays the target this long (a glimpse lost at a door frame is no reason to give up). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Senses", meta = (ClampMin = "0", Units = "Seconds"))
	float TargetMemorySeconds = 0.f;

	/** It investigates a stimulus at most this old (0: however old). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0", Units = "Seconds"))
	float StimulusMemorySeconds = 0.f;

	/** Time it looks around where the stimulus was. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0.01", Units = "Seconds"))
	float InvestigateSeconds = 0.f;

	/** It strikes a target this close, capsule edge to capsule edge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0", Units = "Centimeters"))
	float EngageRange = 0.f;

	/** A chase stops this close, edge to edge (below EngageRange, so the strike is in range when it stops). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0", Units = "Centimeters"))
	float ChaseStopDistance = 0.f;

	/** How often it looks again at an obstacle it opened or waits for. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0.01", Units = "Seconds"))
	float ObstacleRecheckSeconds = 0.f;

	/** Pause after a failed activity (no path, no target) before the brain decides again; also how long a target it could not reach is ignored. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0.01", Units = "Seconds"))
	float RetrySeconds = 0.f;

	/** Its voice as a blow winds up (DA_SoundBank, ADR-0022), on every machine with the wind-up cue. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (Categories = "Hollow.Sound"))
	FGameplayTag WindupSound;

	/**
	 * What it calls out when a player's noise wakes it (POLISH-01, ADR-0053; DA_SoundBank, long range): heard where it is
	 * on every machine, so the squad knows why the hunt began. None: it answers nothing (the Whisperers).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (Categories = "Hollow.Sound"))
	FGameplayTag AcousticAlertSound;

	/** Its footsteps for players (UHollowFootstepAudioComponent); no AI hears them. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (Categories = "Hollow.Sound"))
	FGameplayTag FootstepSound;

	/** Their pace and loudness by speed; heard from PatrolSpeed on (a patrol is never silent). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	FHollowFootstepCadence FootstepCadence;

	/** Gameplay event that triggers its melee attack ability. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (Categories = "Hollow.Event.Enemy"))
	FGameplayTag MeleeEvent;

	/** Gameplay event that triggers its structural blow ability. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (Categories = "Hollow.Event.Enemy"))
	FGameplayTag BreachEvent;

	/** A dead enemy stays this long before it is removed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.01", Units = "Seconds"))
	float DeathLingerSeconds = 0.f;

	/** Share of a shot's or blow's push it takes (GP-01, IHollowImpulseReceiver): 1 flies, 0.25 is the Pilgrim's mass. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0"))
	float KnockbackScale = 0.f;

	/** A push at least this strong (after KnockbackScale) staggers it: StaggerEffect. 0: never staggered. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float StaggerImpulse = 0.f;

	/** A short stun (UHollowStunnedEffect child, GE_Status_Stagger): the brain stops, its attacks are blocked. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	TSubclassOf<UGameplayEffect> StaggerEffect;

	/** Optional montages of its moments (ADR-0027, art pass); empty slots keep the greybox behaviour. Never root motion. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	FHollowEnemyMontages Montages;

	/** Section of an attack montage where the blow lands: its start is fitted to the end of the wind-up. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	FName ImpactSection = TEXT("Impact");

	/** What it may leave when it dies (GP-02). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	FHollowEnemyDrop Drop;
};
