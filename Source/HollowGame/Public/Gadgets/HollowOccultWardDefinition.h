#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"

#include "HollowOccultWardDefinition.generated.h"

class UGameplayEffect;

/**
 * Tuning of the Techno-Occultist's ritual ward (S6-03, ADR-0036): how long the rune circle burns, how far it reaches,
 * how often it looks who is inside and what it does to them; how it shows and sounds.
 */
UCLASS(BlueprintType)
class HOLLOWGAME_API UHollowOccultWardDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	/** The circle burns this long after it is laid, then it is gone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ward", meta = (ClampMin = "0.1", Units = "Seconds"))
	float LifeSeconds = 0.f;

	/** Its radius on the floor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ward", meta = (ClampMin = "1", Units = "Centimeters"))
	float Radius = 0.f;

	/** A body counts as inside up to this far above or below the circle's floor (the middle of its capsule). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ward", meta = (ClampMin = "1", Units = "Centimeters"))
	float HeightTolerance = 0.f;

	/** How often the server looks who is inside (and renews their effects). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ward", meta = (ClampMin = "0.02", Units = "Seconds"))
	float CheckSeconds = 0.f;

	/** On every living enemy inside (GE_Ward_Bind, UHollowWardBindEffect): its duration is how long it lingers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
	TSubclassOf<UGameplayEffect> EnemyEffect;

	/** On every living player inside (GE_Ward_Calm, UHollowWardCalmEffect). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
	TSubclassOf<UGameplayEffect> PlayerEffect;

	/** Instant, Hollow.Data.ResonanceDrain into Resonance (GE_Ward_Drain): applied to every living player inside at each check. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
	TSubclassOf<UGameplayEffect> DrainEffect;

	/** Resonance a player inside loses per second (each check takes its share). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects", meta = (ClampMin = "0"))
	float ResonanceDrainPerSecond = 0.f;

	/** It is laid on the floor found this far below the placer's feet, at the feet otherwise. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ward", meta = (ClampMin = "0", Units = "Centimeters"))
	float PlaceTraceDistance = 0.f;

	/** The circle comes up over this long... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look", meta = (ClampMin = "0", Units = "Seconds"))
	float FadeInSeconds = 0.f;

	/** ...and pulses for this long before it goes out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look", meta = (ClampMin = "0", Units = "Seconds"))
	float PulseSeconds = 0.f;

	/** Pulses per second at the end. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look", meta = (ClampMin = "0.1", Units = "Hertz"))
	float PulseHz = 0.f;

	/** Its hum while it burns (Hollow.Sound.*: players only, the AI does not hear it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look", meta = (Categories = "Hollow.Sound"))
	FGameplayTag HumSound;
};
