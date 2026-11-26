#pragma once

#include "Engine/DataAsset.h"

#include "HollowDroneDefinition.generated.h"

/**
 * Data of the solo drone (DRONE-01, ADR-0054): where it hovers beside its player, how softly it follows, where a load
 * hangs under it and what its prompts say. Every number is here (CLAUDE.md sec. 1); its mesh, light and their look are
 * BP_SoloDrone's components (create_drone_assets.py). AHollowSoloDrone only runs it.
 */
UCLASS(BlueprintType)
class HOLLOWGAME_API UHollowDroneDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/**
	 * Where it hovers, in its player's capsule space (forward, right, up from the capsule centre): beside the view, out of
	 * the line of fire. Walls on the way push it in towards the player (its arm's probe).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Follow")
	FVector FollowOffset = FVector::ZeroVector;

	/** How fast it catches up with the player's moves (the arm's location lag, 1/s: smaller trails further). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Follow", meta = (ClampMin = "0.1"))
	float FollowLagSpeed = 1.f;

	/** How fast it swings round when the player turns (the arm's rotation lag, 1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Follow", meta = (ClampMin = "0.1"))
	float TurnLagSpeed = 1.f;

	/**
	 * How far the player may look away from the drone's heading before it swings round to the new one. Wider than the
	 * hover's own angle (FollowOffset), so the player can turn to face it (and focus it) without chasing it away.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Follow", meta = (ClampMin = "1", ClampMax = "179", Units = "Degrees"))
	float HeadingDeadZone = 1.f;

	/** How often it compares its heading with the player's view (a timer, no Tick). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Follow", meta = (ClampMin = "1", Units = "Hertz"))
	float HeadingRateHz = 1.f;

	/** Furthest it trails behind where it should be. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Follow", meta = (ClampMin = "0", Units = "Centimeters"))
	float MaxLagDistance = 0.f;

	/** Radius of the probe that keeps it out of walls. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Follow", meta = (ClampMin = "1", Units = "Centimeters"))
	float ProbeRadius = 1.f;

	/** Radius of the sphere its player focuses to hand it a load or take it back (the Interaction channel). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (ClampMin = "1", Units = "Centimeters"))
	float FocusRadius = 1.f;

	/** Where a carried load hangs, in the drone's own (unscaled) space: below it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry")
	FVector CarryPointOffset = FVector::ZeroVector;

	/** Focusing it while carrying a core alone (string table entry, LOC-01). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText HandOverPrompt;

	/** Focusing it while it carries the player's core. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText TakeBackPrompt;
};
