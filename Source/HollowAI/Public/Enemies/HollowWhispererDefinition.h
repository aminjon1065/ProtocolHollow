#pragma once

#include "Enemies/HollowEnemyDefinition.h"

#include "HollowWhispererDefinition.generated.h"

class UEnvQuery;

/**
 * Tuning of a Whisperer (GP-02, ADR-0047), on top of an enemy definition: a small, fast swarm creature that fears the
 * players' lights. A beam that catches it makes it freeze with a shriek and back away (LitEffect slows it while the
 * light stays); it flanks through the dark to its prey's side or back (FlankQuery) and lunges from there; in the dark,
 * with its prey's light off, the swarm goes into a frenzy (FrenzyEffect) and every one of it bites. PatrolSpeed is its
 * idle scuttle by its nest, ChaseSpeed its chase and flank, MeleeEvent triggers its bite. It opens plain doors but
 * breaks nothing: a welded door keeps it out.
 */
UCLASS(BlueprintType)
class HOLLOWAI_API UHollowWhispererDefinition : public UHollowEnemyDefinition
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	virtual bool CanBreach() const override { return false; }

	/** Renewed on it by every light check while a beam lights it (GE_Whisperer_Lit: Hollow.State.Lit and the slow). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Whisperer")
	TSubclassOf<UGameplayEffect> LitEffect;

	/** Renewed on it while it is in the dark and its prey has no light (GE_Whisperer_Frenzy: Hollow.State.Frenzy, the haste). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Whisperer")
	TSubclassOf<UGameplayEffect> FrenzyEffect;

	/** It freezes this long when a beam catches it (its shriek), a bite being wound up undone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Whisperer", meta = (ClampMin = "0.01", Units = "Seconds"))
	float FlinchSeconds = 0.f;

	/** How far it then backs away from the light that caught it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Whisperer", meta = (ClampMin = "0", Units = "Centimeters"))
	float RecoilDistance = 0.f;

	/** How far it flees an Aether lantern's beam (Hollow.State.Exposed) instead. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Whisperer", meta = (ClampMin = "0", Units = "Centimeters"))
	float ExposedFleeDistance = 0.f;

	/** A light catching it again this soon does not make it flinch again (a lamp flicked off and on cannot pin it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Whisperer", meta = (ClampMin = "0", Units = "Seconds"))
	float RecoilCooldownSeconds = 0.f;

	/** A spot by its prey that no light reaches, reachable, nearest to it (EQ_WhispererFlank). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Whisperer")
	TObjectPtr<UEnvQuery> FlankQuery;

	/** Having reached the dark, it lunges at its prey this long before it would flank again. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Whisperer", meta = (ClampMin = "0.01", Units = "Seconds"))
	float LungeSeconds = 0.f;

	/** In the dark spot but not its turn to bite (the others bite): it waits this long before it moves again. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Whisperer", meta = (ClampMin = "0.01", Units = "Seconds"))
	float FlankPauseSeconds = 0.f;

	/** A player with a light counts as this many times farther when it picks whom to hunt: it goes for the one in the dark. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Whisperer", meta = (ClampMin = "1"))
	float LitPreyDistanceScale = 1.f;

	/** Volume of its whisper in a frenzy (1: as it always whispers). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0"))
	float FrenzyWhisperVolume = 1.f;
};
