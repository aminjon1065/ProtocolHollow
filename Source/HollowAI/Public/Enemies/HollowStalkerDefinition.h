#pragma once

#include "Curves/CurveFloat.h"
#include "Enemies/HollowEnemyDefinition.h"

#include "HollowStalkerDefinition.generated.h"

class UEnvQuery;

/**
 * Tuning of the Echo-Stalker (ADR-0019), on top of an enemy definition: its own brain moves (shadowing, ambush,
 * retreat) and the query that finds a dark spot in a room. PatrolSpeed is its shadowing speed, ChaseSpeed its hunt
 * speed, MeleeEvent triggers its ambush strike.
 */
UCLASS(BlueprintType)
class HOLLOWAI_API UHollowStalkerDefinition : public UHollowEnemyDefinition
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	/** It opens doors but breaks nothing: what is welded or shut it goes around, or through the vents. */
	virtual bool CanBreach() const override { return false; }

	/** A spot in the focus room (the ambush or shadowing room) outside every lamp's cone, reachable, near the hottest portal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker")
	TObjectPtr<UEnvQuery> FocusQuery;

	/** It waits in ambush at most this long before it picks again. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (ClampMin = "0.01", Units = "Seconds"))
	float AmbushWaitSeconds = 0.f;

	/** A living player this close (edge to edge, in sight) springs the ambush. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (ClampMin = "0", Units = "Centimeters"))
	float AmbushTriggerRange = 0.f;

	/** How often it looks for a victim while it waits. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (ClampMin = "0.05", Units = "Seconds"))
	float AmbushCheckSeconds = 0.f;

	/** Pause in its shadowing spot before it follows the squad again. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker", meta = (ClampMin = "0.01", Units = "Seconds"))
	float TrackPauseSeconds = 0.f;

	/**
	 * Its hearing range is HearingRange times this, by the squad's Resonance (0..1, ADR-0020): the deeper the squad is in
	 * the Aether, the farther it hears them. 1 at 0, never decreasing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker")
	FRuntimeFloatCurve HearingResonanceCurve;
};
