#pragma once

#include "Components/ActorComponent.h"

#include "HollowFootstepNoiseComponent.generated.h"

/**
 * Footstep noise of a player character (ADR-0018), server only: every UHollowNoiseConfig::FootstepInterval it reports
 * Hollow.Noise.Footstep at the feet, louder in proportion to the speed on the ground. Standing, crouching, falling
 * and the dead are silent. A timer (no Tick); the sound players hear is the animation's business (S4-04).
 */
UCLASS(ClassGroup = (Hollow), meta = (BlueprintSpawnableComponent))
class HOLLOWGAME_API UHollowFootstepNoiseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	explicit UHollowFootstepNoiseComponent(const FObjectInitializer& ObjectInitializer);

	/** One footstep check now (the timer's step; tests call it directly). True if a footstep was reported. */
	bool Step();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleStepTimer();

	FTimerHandle StepTimer;
};
