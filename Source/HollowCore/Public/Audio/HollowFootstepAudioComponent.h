#pragma once

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"

#include "HollowFootstepAudioComponent.generated.h"

/** Pace and loudness of footsteps no AI hears (an enemy's): a step every Interval while faster than MinSpeed. */
USTRUCT(BlueprintType)
struct HOLLOWCORE_API FHollowFootstepCadence
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps", meta = (ClampMin = "0", Units = "Seconds"))
	float Interval = 0.f;

	/** Slower than this is silent (turning on the spot, shuffling). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float MinSpeed = 0.f;

	/** Speed at full volume; slower steps are quieter in proportion. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float ReferenceSpeed = 0.f;

	bool IsValid() const { return Interval > 0.f && ReferenceSpeed > 0.f && MinSpeed <= ReferenceSpeed; }
	/** Volume of a step at Speed: 0 under MinSpeed, Speed / ReferenceSpeed above. */
	float VolumeAt(float Speed) const { return !IsValid() || Speed < MinSpeed ? 0.f : Speed / ReferenceSpeed; }
};

/**
 * Footsteps players hear (ADR-0022), on every machine but a dedicated server, for any character (others' too: their
 * velocity and movement mode replicate). A Hollow.Noise.* step (a player's) keeps the noise footsteps' cadence and
 * loudness by speed (DA_NoiseConfig), so what one hears matches what the AI hears; a Hollow.Sound.* one (an enemy's,
 * which no AI hears) has its own Cadence.
 */
UCLASS(ClassGroup = (Hollow), meta = (BlueprintSpawnableComponent))
class HOLLOWCORE_API UHollowFootstepAudioComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	explicit UHollowFootstepAudioComponent(const FObjectInitializer& ObjectInitializer);

	/** One step now if the owner walks (the timer's; tests call it). Returns whether it stepped. */
	bool Step();

	/** Hollow.Noise.Footstep for players; enemies have their own Hollow.Sound.Footstep.* (no AI hears them). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hollow|Audio", meta = (Categories = "Hollow.Noise,Hollow.Sound"))
	FGameplayTag SoundTag;

	/** Pace of a Hollow.Sound.* step (unused for a noise: the noise rules decide). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hollow|Audio")
	FHollowFootstepCadence Cadence;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool IsNoise() const;
	/** Seconds between steps, 0 if it never steps. */
	float GetInterval() const;
	float VolumeAt(float Speed) const;

	FTimerHandle StepTimer;
};
