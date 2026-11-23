#pragma once

#include "GameplayCueNotify_Actor.h"

#include "HollowLanternCue.generated.h"

class APawn;
class UAudioComponent;
class UHollowLanternDefinition;
class USpotLightComponent;

/**
 * Light cone of a running Aether lantern (GameplayCue.Hollow.Tool.Lantern, S6-02), purely cosmetic and local to every
 * machine: a spot light from the wielder's muzzle (the held tool's; else MuzzleOffset from the eyes) along its aim
 * (owners: control rotation, others: replicated aim), shaped by the lantern (cone, range, colour), no shadows (light
 * budget). Its hum is the lantern's noise tag (ADR-0022). Updated by a timer only while active (no Tick).
 */
UCLASS(Abstract)
class HOLLOWGAME_API AHollowLanternCue : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	explicit AHollowLanternCue(const FObjectInitializer& ObjectInitializer);

	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;

	USpotLightComponent* GetLight() const { return Light; }

	/** The cone's brightness now: its steady brightness, times the flare while it settles (AK-8). */
	float GetIntensity() const;
	float GetFlashIntensityScale() const { return FlashIntensityScale; }
	float GetFlashSeconds() const { return FlashSeconds; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Lantern")
	TObjectPtr<USpotLightComponent> Light;

	/** Light refresh rate while the lantern runs. */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Lantern", meta = (ClampMin = "1", Units = "Hertz"))
	float UpdateRateHz = 0.f;

	/** Where the light leaves while no held tool has a muzzle: from the eyes in view space (forward, right, up). */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Lantern")
	FVector MuzzleOffset = FVector::ZeroVector;

	/** The bright core of the cone, as a fraction of the lantern's half angle. */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Lantern", meta = (ClampMin = "0", ClampMax = "1"))
	float InnerConeFraction = 0.f;

	/**
	 * AK-8 (ADR-0043): the lantern flares as it lights: the cone starts FlashIntensityScale times as bright and settles
	 * over FlashSeconds (on the light's own refresh timer). None with reduced flashes (H-1, ADR-0029).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Lantern", meta = (ClampMin = "1"))
	float FlashIntensityScale = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Lantern", meta = (ClampMin = "0", Units = "Seconds"))
	float FlashSeconds = 0.f;

private:
	void UpdateLight();

	TWeakObjectPtr<APawn> Wielder;
	TWeakObjectPtr<const UHollowLanternDefinition> Lantern;
	FTimerHandle UpdateTimer;
	/** The cone's steady brightness (the class default's, so a flare never compounds) and the world time it lit at. */
	float BaseIntensity = 0.f;
	double LitAt = 0.0;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Sound;
};
