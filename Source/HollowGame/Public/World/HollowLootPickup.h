#pragma once

#include "GameFramework/Actor.h"
#include "Interfaces/HollowInteractable.h"

#include "HollowLootPickup.generated.h"

class UBoxComponent;
class UHollowItemDefinition;
class UStaticMeshComponent;
struct FStreamableHandle;

/**
 * Loot on the floor (GP-01b, ADR-0045): Count of an item a broken crate spilled. A physics body on every machine (the
 * server's leads, its movement replicates while it moves; once it sleeps the pickup goes dormant), on HollowPickup:
 * it rests on the world and props but never pushes a pawn, and shots pass through. Interact (E, the gamepad's X held)
 * puts it into the player's raid inventory as found loot (UHollowInventoryComponent::AddFoundItem): the server checks
 * the request like any interaction. It shows the item's PickupMesh. Never ticks.
 */
UCLASS()
class HOLLOWGAME_API AHollowLootPickup : public AActor, public IHollowInteractable
{
	GENERATED_BODY()

public:
	explicit AHollowLootPickup(const FObjectInitializer& ObjectInitializer);

	/** Server, before FinishSpawning. */
	void SetLoot(UHollowItemDefinition* InItem, int32 InCount);

	/** Server: thrown out of its crate at Velocity. */
	void Launch(const FVector& Velocity);

	const UHollowItemDefinition* GetItem() const { return Item; }
	int32 GetCount() const { return Count; }
	bool IsTaken() const { return bTaken; }

	//~ IHollowInteractable
	virtual FHollowInteractionInfo GetInteractionInfo(const AActor* Interactor) const override;
	virtual bool CanInteract(const AActor* Interactor) const override;
	virtual void Interact(AActor* Interactor) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** The physics body (its extent is data): the kit's meshes are looks only. */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Loot")
	TObjectPtr<UBoxComponent> Body;

	/** Its look (no collision): the item's pickup mesh. */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Loot")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** The server looks this often whether it came to rest (then it stops replicating its movement). */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Loot", meta = (ClampMin = "0.05", Units = "Seconds"))
	float RestCheckSeconds = 0.5f;

private:
	UFUNCTION()
	void OnRep_Loot();

	UFUNCTION()
	void OnRep_Taken();

	/** The item's look (its pickup mesh, loaded asynchronously; the class's own mesh without one). */
	void ApplyLook();
	void CheckRest();

	UPROPERTY(ReplicatedUsing = OnRep_Loot)
	TObjectPtr<UHollowItemDefinition> Item;

	UPROPERTY(ReplicatedUsing = OnRep_Loot)
	int32 Count = 0;

	/** Picked up: gone on every machine (with its sound), destroyed a moment later on the server. */
	UPROPERTY(ReplicatedUsing = OnRep_Taken)
	bool bTaken = false;

	FTimerHandle RestTimer;
	TSharedPtr<FStreamableHandle> LookLoad;
};
