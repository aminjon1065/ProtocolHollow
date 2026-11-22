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

private:
	void UpdateLight();

	TWeakObjectPtr<APawn> Wielder;
	TWeakObjectPtr<const UHollowLanternDefinition> Lantern;
	FTimerHandle UpdateTimer;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Sound;
};
