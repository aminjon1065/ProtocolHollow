#pragma once

#include "GameFramework/Actor.h"
#include "Interfaces/HollowInteractable.h"

#include "HollowDatapad.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/**
 * A crew member's datapad on the floor of a sector room (NAR-01b, ADR-0050), spawned by the server at a Loot socket
 * (the biome's socket rule, two per raid). Interact (E) downloads it: the server picks the crew log it holds
 * (UHollowLoreSubsystem) and the whole squad gets it; the pad goes. Its screen glows by its material: no light, no
 * Tick. BP_Datapad (create_story_assets.py) gives the mesh and the prompt.
 */
UCLASS()
class HOLLOWGAME_API AHollowDatapad : public AActor, public IHollowInteractable
{
	GENERATED_BODY()

public:
	explicit AHollowDatapad(const FObjectInitializer& ObjectInitializer);

	bool IsTaken() const { return bTaken; }

	//~ IHollowInteractable
	virtual FHollowInteractionInfo GetInteractionInfo(const AActor* Interactor) const override;
	virtual bool CanInteract(const AActor* Interactor) const override;
	virtual void Interact(AActor* Interactor) override;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** What the interaction trace hits (pickup profile: never blocks a pawn, shots pass through). */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Datapad")
	TObjectPtr<UBoxComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Datapad")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** "Download crew log" (an ST_World entry). */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Datapad")
	FText Prompt;

private:
	UFUNCTION()
	void OnRep_Taken();

	UPROPERTY(ReplicatedUsing = OnRep_Taken)
	bool bTaken = false;
};
