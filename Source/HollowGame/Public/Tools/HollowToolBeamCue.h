#pragma once

#include "GameplayCueNotify_Actor.h"

#include "HollowToolBeamCue.generated.h"

class APawn;
class UHollowToolDefinition;
class UAudioComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * Looping beam of a continuous tool (GameplayCue.Hollow.Tool.Beam), purely cosmetic and local to every machine: a
 * stretched mesh from the wielder's muzzle to its own trace hit, plus a glow at the hit. Remote wielders aim with
 * their replicated base aim rotation, so no hit point is replicated. Updated by a timer only while active (no Tick).
 * Greybox visual; Niagara replaces the mesh on the art pass (GCN_Tool_Beam is data).
 */
UCLASS(Abstract)
class HOLLOWGAME_API AHollowToolBeamCue : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	explicit AHollowToolBeamCue(const FObjectInitializer& ObjectInitializer);

	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Beam")
	TObjectPtr<UStaticMeshComponent> Beam;

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Beam")
	TObjectPtr<UPointLightComponent> Glow;

	/** Beam refresh rate while the tool runs. */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Beam", meta = (ClampMin = "1", Units = "Hertz"))
	float UpdateRateHz = 0.f;

	/** Vector parameter of the beam material tinted with the tool's BeamColor. */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Beam")
	FName ColorParameter;

	/** Where the beam leaves the tool, from the eyes in view space (forward, right, up). */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Beam")
	FVector MuzzleOffset = FVector::ZeroVector;

private:
	void UpdateBeam();

	TWeakObjectPtr<APawn> Wielder;
	TWeakObjectPtr<const UHollowToolDefinition> Tool;
	FTimerHandle UpdateTimer;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BeamMaterial;

	/** The tool's sound while it runs: its noise tag (the AI hears the same, ADR-0022). */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Sound;
};
