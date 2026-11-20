#pragma once

#include "GameplayTagContainer.h"

#include "HollowNoiseTypes.generated.h"

class AActor;

/** A noise made in the world (Docs/Architecture/03 sec. 3.8, ADR-0018): reported on the server to UHollowNoiseSubsystem. */
USTRUCT(BlueprintType)
struct HOLLOWCORE_API FHollowNoiseEvent
{
	GENERATED_BODY()

	FHollowNoiseEvent() = default;
	FHollowNoiseEvent(const FGameplayTag& InNoiseTag, const FVector& InLocation, AActor* InInstigator, const float InLoudnessScale = 1.f)
		: NoiseTag(InNoiseTag), Location(InLocation), Instigator(InInstigator), LoudnessScale(InLoudnessScale)
	{
	}

	/** Hollow.Noise.*: picks the rule (loudness, range, rate) in UHollowNoiseConfig. */
	UPROPERTY(BlueprintReadWrite, Category = "Noise", meta = (Categories = "Hollow.Noise"))
	FGameplayTag NoiseTag;

	UPROPERTY(BlueprintReadWrite, Category = "Noise")
	FVector Location = FVector::ZeroVector;

	/** Who made it: hearing attributes the stimulus to it (enemies react to noise made by living players). */
	UPROPERTY(BlueprintReadWrite, Category = "Noise")
	TWeakObjectPtr<AActor> Instigator;

	/** Multiplies the rule's loudness (footsteps by speed, later the voice by microphone level). */
	UPROPERTY(BlueprintReadWrite, Category = "Noise")
	float LoudnessScale = 1.f;
};

/** A noise as it went out to AI hearing, after its rule. */
struct FHollowNoiseReport
{
	FHollowNoiseEvent Event;
	/** Multiplies a listener's hearing range. */
	float Loudness = 0.f;
	/** Heard no farther than this (0: only the hearing range limits it). */
	float MaxRange = 0.f;
};
