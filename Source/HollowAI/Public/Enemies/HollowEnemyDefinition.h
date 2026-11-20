#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "HollowEnemyDefinition.generated.h"

class UHollowAbilitySet;
class UNavigationQueryFilter;
class UStateTree;

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

	/** Gameplay event that triggers its melee attack ability. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (Categories = "Hollow.Event.Enemy"))
	FGameplayTag MeleeEvent;

	/** Gameplay event that triggers its structural blow ability. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (Categories = "Hollow.Event.Enemy"))
	FGameplayTag BreachEvent;

	/** A dead enemy stays this long before it is removed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.01", Units = "Seconds"))
	float DeathLingerSeconds = 0.f;
};
