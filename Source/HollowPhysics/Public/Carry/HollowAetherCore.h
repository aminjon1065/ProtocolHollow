#pragma once

#include "GameFramework/Actor.h"
#include "Interfaces/HollowInteractable.h"

#include "HollowAetherCore.generated.h"

class UBoxComponent;
class UHollowCarryableComponent;
class UMaterialInterface;
class UStaticMeshComponent;

/**
 * Heavy Aether core (ADR-0015): the raid's loot, carried with both hands. A box body (root: physics, collision,
 * Predictive Interpolation while dropped) and a visual mesh that each machine attaches locally while it is carried.
 * Spawned by the server at a CoreSpawn socket, resting (kinematic, dormant). Lifting goes through the normal
 * interaction (UHollowInteractionComponent), and so does a second player taking the other side of a carried core
 * (ADR-0028); all carry logic lives in UHollowCarryableComponent. No Tick.
 */
UCLASS()
class HOLLOWPHYSICS_API AHollowAetherCore : public AActor, public IHollowInteractable
{
	GENERATED_BODY()

public:
	explicit AHollowAetherCore(const FObjectInitializer& ObjectInitializer);

	UBoxComponent* GetBody() const { return Body; }
	UStaticMeshComponent* GetVisual() const { return Visual; }
	UHollowCarryableComponent* GetCarryable() const { return Carryable; }

	// IHollowInteractable
	virtual FHollowInteractionInfo GetInteractionInfo(const AActor* Interactor) const override;
	virtual bool CanInteract(const AActor* Interactor) const override;
	virtual void Interact(AActor* Interactor) override;

	virtual void PostInitializeComponents() override;
	virtual void OnRep_ReplicateMovement() override;

protected:
	/** Every machine: the core hums (Hollow.Sound.Core.Hum), wherever it is carried; it looks like the item it counts as. */
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Carry")
	TObjectPtr<UBoxComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Carry")
	TObjectPtr<UStaticMeshComponent> Visual;

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Carry")
	TObjectPtr<UHollowCarryableComponent> Carryable;

	/** Every machine: the visual's material for the item it counts as now (a definition look, else its own). */
	void ApplyLook();

	/** The visual's own material (the plain core). */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> DefaultMaterial;
};
