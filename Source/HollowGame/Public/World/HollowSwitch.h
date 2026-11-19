#pragma once

#include "GameFramework/Actor.h"
#include "Interfaces/HollowInteractable.h"

#include "HollowSwitch.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

/**
 * Generic two-state switch (sector power, lights, airlock panels). The server flips the replicated state; every
 * machine shows it through OnRep (CLAUDE.md sec. 3). Level-placed switches stay dormant until first used.
 * Mesh, lamp and prompts are data (BP_Switch).
 */
UCLASS()
class HOLLOWGAME_API AHollowSwitch : public AActor, public IHollowInteractable
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnHollowSwitchToggled, bool /*bIsOn*/);

	explicit AHollowSwitch(const FObjectInitializer& ObjectInitializer);

	bool IsOn() const { return bIsOn; }
	UStaticMeshComponent* GetMesh() const { return Mesh; }
	UPointLightComponent* GetLamp() const { return Lamp; }

	/** Fired on every machine when the state changes (server: on toggle, clients: on replication). */
	FOnHollowSwitchToggled OnToggled;

	// IHollowInteractable
	virtual FHollowInteractionInfo GetInteractionInfo(const AActor* Interactor) const override;
	virtual bool CanInteract(const AActor* Interactor) const override;
	virtual void Interact(AActor* Interactor) override;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_IsOn();

	/** Cosmetic presentation of the current state (lamp). */
	void ApplyState();

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Switch")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Switch")
	TObjectPtr<UPointLightComponent> Lamp;

	UPROPERTY(EditAnywhere, Category = "Hollow|Switch")
	FText PromptTurnOn;

	UPROPERTY(EditAnywhere, Category = "Hollow|Switch")
	FText PromptTurnOff;

	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_IsOn, Category = "Hollow|Switch")
	bool bIsOn = false;
};
