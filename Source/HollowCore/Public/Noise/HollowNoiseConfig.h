#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "HollowNoiseConfig.generated.h"

struct FHollowSectorTopology;

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

	/**
	 * An enemy that wakes to this noise answers it (POLISH-01, ADR-0053): its acoustic alert sounds where it is and the
	 * visor of the player who made the noise says so. Off for the steady ones (footsteps, the lantern's hum).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise")
	bool bRaisesAcousticAlert = false;
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

	/**
	 * A noise from another room (the sector topology) carries this fraction of its range for the AI (S4-04, ADR-0022);
	 * players hear it muffled the same way (each room's acoustics volume filters what comes from outside). 1: no walls.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rooms", meta = (ClampMin = "0", ClampMax = "1"))
	float InterRoomRange = 1.f;

	/** One enemy answers a noise at most this often (POLISH-01): a welder going on does not keep it calling out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Acoustic alert", meta = (ClampMin = "0", Units = "Seconds"))
	float AnswerInterval = 0.f;

	/** A player's visor shows an acoustic alert at most this often, whoever answered (POLISH-01: no spam). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Acoustic alert", meta = (ClampMin = "0", Units = "Seconds"))
	float AcousticAlertCooldown = 0.f;

	/** How far a noise reaches a listener hearing HearingRange: Loudness x LoudnessScale x HearingRange, capped by its MaxRange. */
	float RangeFor(const FHollowNoiseRule& Rule, float HearingRange, float LoudnessScale = 1.f) const;

	/** Whether a noise Distance away reaches that listener: within RangeFor, InterRoomRange of it from another room. */
	bool Reaches(const FHollowNoiseRule& Rule, float HearingRange, float LoudnessScale, float Distance, bool bFromAnotherRoom) const;

	/**
	 * The walls of the sector (ADR-0022): whether a noise at Source reaches a listener at Listener. From another room of
	 * Topology only within InterRoomRange of its range; in the same room or outside every room the hearing sense decides.
	 */
	bool ReachesThroughWalls(const FHollowSectorTopology& Topology, const FHollowNoiseRule& Rule, float HearingRange, float LoudnessScale,
		const FVector& Source, const FVector& Listener) const;
};
