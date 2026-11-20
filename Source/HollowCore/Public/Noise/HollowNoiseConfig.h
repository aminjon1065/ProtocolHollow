#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "HollowNoiseConfig.generated.h"

/** How loud one kind of noise is (DA_NoiseConfig). */
USTRUCT(BlueprintType)
struct FHollowNoiseRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise", meta = (Categories = "Hollow.Noise"))
	FGameplayTag NoiseTag;

	/** Multiplies a listener's hearing range: 1 is heard as far as it hears. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise", meta = (ClampMin = "0"))
	float Loudness = 0.f;

	/** Heard no farther than this, however loud (0: no limit but the hearing range). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise", meta = (ClampMin = "0", Units = "Centimeters"))
	float MaxRange = 0.f;

	/** At most one event per instigator this often (continuous sources such as tools; 0: every event). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise", meta = (ClampMin = "0", Units = "Seconds"))
	float MinInterval = 0.f;
};

/**
 * Every number of noise -> AI (Docs/Architecture/03 sec. 3.8, ADR-0018): one rule per Hollow.Noise.* tag and the
 * footstep cadence. Data (DA_NoiseConfig, chosen by UHollowNoiseSettings), so hearing is tuned without a rebuild.
 */
UCLASS(BlueprintType)
class HOLLOWCORE_API UHollowNoiseConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** The rule of a tag, or null. */
	const FHollowNoiseRule* FindRule(const FGameplayTag& NoiseTag) const;

	/** Loudness scale of a footstep at Speed on the ground: 0 below FootstepMinSpeed, then Speed / FootstepReferenceSpeed. */
	float FootstepLoudnessScale(float Speed) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules", meta = (TitleProperty = "NoiseTag"))
	TArray<FHollowNoiseRule> Rules;

	/** A walking player is checked for a footstep this often. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps", meta = (ClampMin = "0.05", Units = "Seconds"))
	float FootstepInterval = 0.f;

	/** Slower than this is silent (crouch-walking, shuffling). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float FootstepMinSpeed = 0.f;

	/** Speed of a footstep at the rule's loudness (walking); sprinting is louder in proportion. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps", meta = (ClampMin = "1", Units = "CentimetersPerSecond"))
	float FootstepReferenceSpeed = 0.f;
};
