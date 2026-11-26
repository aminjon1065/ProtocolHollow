#pragma once

#include "Carry/HollowCarryVehicle.h"
#include "GameFramework/Actor.h"
#include "Interfaces/HollowCarryReceiver.h"
#include "Interfaces/HollowInteractable.h"

#include "HollowSoloDrone.generated.h"

class AHollowExoFrameCharacter;
class UAudioComponent;
class UHollowCarrierComponent;
class UHollowCarryableComponent;
class UHollowDroneDefinition;
class USphereComponent;
class USpotLightComponent;
class USpringArmComponent;
class UStaticMeshComponent;

/**
 * The tactical solo drone (DRONE-01, ADR-0054). The server spawns one for a raid of exactly one player
 * (UHollowSoloDroneSubsystem), never in co-op. It hovers beside its player's view: attached to the capsule, it rides a
 * spring arm whose lag smooths the follow and whose probe keeps it out of walls (the engine component moves it; no Tick
 * of its own). Its heading is its own: it swings round only when the player looks further away than HeadingDeadZone
 * (a timer), so turning to face it does not chase it away. A soft unshadowed spotlight and a quiet procedural hum (Hollow.Sound.Drone.Hum, players only: no noise
 * rule, no AI hears or sees it) come with it; it never blocks pawns or shots.
 *
 * Carrying (EHollowCarryMode::Drone): the player, carrying an Aether core alone, focuses the drone and presses Interact
 * (a carry receiver): the core hangs under the drone, the player moves at the drone mode's pace with both hands free for
 * tools and weapons. The player stays the core's carrier, so the Purification Altar takes it and the extraction counts
 * it. Interact on the drone again takes it back. The player dying drops it from under the drone and puts the drone dark.
 */
UCLASS()
class HOLLOWGAME_API AHollowSoloDrone : public AActor, public IHollowInteractable, public IHollowCarryReceiver, public IHollowCarryVehicle
{
	GENERATED_BODY()

public:
	explicit AHollowSoloDrone(const FObjectInitializer& ObjectInitializer);

	/** Server, before BeginPlay (the subsystem spawns it deferred): the player it serves. */
	void SetPlayer(AHollowExoFrameCharacter* InPlayer);
	AHollowExoFrameCharacter* GetPlayer() const { return Player; }

	// IHollowInteractable
	virtual FHollowInteractionInfo GetInteractionInfo(const AActor* Interactor) const override;
	virtual bool CanInteract(const AActor* Interactor) const override;
	virtual void Interact(AActor* Interactor) override;

	// IHollowCarryVehicle
	virtual USceneComponent* GetCarryPoint() const override;
	virtual APawn* GetCarryOwner() const override;

	/** What it carries for its player now, or null. */
	UHollowCarryableComponent* GetCarried() const;
	/** Whether its player could focus it now (the Interaction channel blocks only then, so it never hides what lies behind). */
	bool IsFocusable() const;
	bool IsLit() const;
	bool IsHumming() const;
	const UHollowDroneDefinition* GetDefinition() const { return Definition; }
	USpringArmComponent* GetArm() const { return Arm; }
	USceneComponent* GetHover() const { return Hover; }
	USpotLightComponent* GetLight() const { return Light; }
	FString Describe() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_Player();

	/** The definition onto the arm, the hover frame, the focus sphere and the carry point. */
	void ApplyDefinition();
	/** Every machine: rides its player's capsule and listens to its carrying and its death. */
	void BindToPlayer();
	void UnbindFromPlayer();
	void HandleCarriedChanged(const UHollowCarrierComponent& Carrier);
	void HandlePlayerDied(AHollowExoFrameCharacter& Character);
	UFUNCTION()
	void HandlePlayerEndPlay(AActor* Actor, EEndPlayReason::Type Reason);
	void RefreshFocusable();
	/** Timer: a new heading once the player's view left the dead zone round the current one. */
	void UpdateHeading();
	/** Light and hum off: its player died. */
	void GoDark();
	/** What Interactor carries, if Interactor is this drone's player. */
	UHollowCarryableComponent* CarriedBy(const AActor* Interactor) const;
	bool CanTakeBackFrom(const UHollowCarryableComponent& Carryable, const APawn* Pawn) const;

	UPROPERTY(EditDefaultsOnly, Category = "Drone")
	TObjectPtr<UHollowDroneDefinition> Definition;

	UPROPERTY(VisibleAnywhere, Category = "Drone")
	TObjectPtr<USceneComponent> Root;

	/** Lagged follow and wall probe: points from the hover spot back to the player (see ApplyDefinition). */
	UPROPERTY(VisibleAnywhere, Category = "Drone")
	TObjectPtr<USpringArmComponent> Arm;

	/** At the arm's end, turned back to the player's facing: the mesh, the light and the carry point hang from it. */
	UPROPERTY(VisibleAnywhere, Category = "Drone")
	TObjectPtr<USceneComponent> Hover;

	/** The drone's look (BP_SoloDrone data: Fab mesh, scale). No collision. */
	UPROPERTY(VisibleAnywhere, Category = "Drone")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** What its player focuses (Interaction channel only, while focusable). */
	UPROPERTY(VisibleAnywhere, Category = "Drone")
	TObjectPtr<USphereComponent> Focus;

	/** Soft spotlight ahead (BP_SoloDrone data); never casts shadows (the light budget). */
	UPROPERTY(VisibleAnywhere, Category = "Drone")
	TObjectPtr<USpotLightComponent> Light;

	/** Where a carried load hangs. */
	UPROPERTY(VisibleAnywhere, Category = "Drone")
	TObjectPtr<USceneComponent> CarryPoint;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Hum;

	UPROPERTY(ReplicatedUsing = OnRep_Player)
	TObjectPtr<AHollowExoFrameCharacter> Player;

	TWeakObjectPtr<AHollowExoFrameCharacter> BoundPlayer;
	FTimerHandle HeadingTimer;
	/** The arm's rotation in the drone's own heading frame: from the hover spot back to the player. */
	FRotator Back = FRotator::ZeroRotator;
	TOptional<double> Heading;
	FDelegateHandle CarriedHandle;
	FDelegateHandle DiedHandle;
	bool bDark = false;
};
