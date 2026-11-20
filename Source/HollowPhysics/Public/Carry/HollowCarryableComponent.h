#pragma once

#include "Carry/HollowCarryTypes.h"
#include "Components/ActorComponent.h"

#include "HollowCarryableComponent.generated.h"

class APawn;
class UHollowCarryableDefinition;
class UPrimitiveComponent;

/** How this machine shows a carryable right now (local, for logs and tests). */
UENUM()
enum class EHollowCarryPresentation : uint8
{
	/** Free and kinematic: lying still, not replicating movement (dormant). */
	Resting,
	/** Free and simulating after a drop (Predictive Interpolation on clients). */
	Awake,
	/** Carried by the pawn this machine views through: visual on the carrier's first-person attach component. */
	FirstPerson,
	/** Carried by a pawn this machine sees from outside: visual on the carrier's body. */
	ThirdPerson
};

/**
 * A heavy object players carry (Docs/Architecture/03 sec. 3.5, ADR-0015). The server owns FHollowCarryNetState
 * (push model; the owner is dormant except while a dropped body is awake). While carried, the body rides on the
 * carrier's capsule at GripOffset (the server attaches it, the engine's attachment replication repeats it) and every
 * machine attaches the visual mesh locally: the carrier's own view to its first-person component, everyone else to
 * the carrier's smoothed body. Dropped: the server simulates the body (Predictive Interpolation); once asleep it turns
 * kinematic, stops replicating movement and goes dormant, and clients settle on the replicated rest pose.
 * The carrier's side (effects, movement claim) is UHollowCarrierComponent; no Tick.
 */
UCLASS(ClassGroup = (Hollow), meta = (BlueprintSpawnableComponent))
class HOLLOWPHYSICS_API UHollowCarryableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Old and new replicated state, on every machine (the server right after a change, clients on replication). */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnHollowCarryStateChanged, const FHollowCarryNetState& /*Old*/, const FHollowCarryNetState& /*New*/);

	explicit UHollowCarryableComponent(const FObjectInitializer& ObjectInitializer);

	/** The owner's body (its root primitive: physics, collision, replicated movement) and visual mesh (attached to it). */
	void SetComponents(UPrimitiveComponent* InBody, UPrimitiveComponent* InVisual);

	const UHollowCarryableDefinition* GetDefinition() const { return Definition; }
	const FHollowCarryNetState& GetCarryState() const { return CarryState; }
	UPrimitiveComponent* GetBody() const { return Body.Get(); }
	UPrimitiveComponent* GetVisual() const { return Visual.Get(); }
	EHollowCarryPresentation GetPresentation() const;

	/** Whether Carrier could lift this alone now: free, and the carrier holds nothing (local focus and server check). */
	bool CanBeginSolo(const APawn* Carrier) const;

	/** Server: Carrier lifts this alone. Its UHollowCarrierComponent learns it through the state change. */
	bool BeginSolo(APawn* Carrier);

	/**
	 * Server: puts this down; it simulates from Velocity until it sleeps. It goes where it fits around the carrier
	 * (FindDropLocation); with no room anywhere it stays carried (false), unless bForce (the carrier is leaving: its own
	 * space is used).
	 */
	bool Release(const FVector& Velocity, bool bForce = false);

	/** Server: the dropped body fell asleep: final pose now, rest and dormancy after DormancyDelaySeconds. */
	void HandleBodySleep();

	/** Every machine: the owner's movement replication changed (AActor::OnRep_ReplicateMovement on clients). */
	void HandleMovementReplicationChanged();

	/** Clients: a carrier pawn is leaving this machine before the server's drop arrives: show this as free meanwhile. */
	void HandleCarrierEndPlay(const APawn* Carrier);

	FOnHollowCarryStateChanged OnCarryStateChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, Category = "Hollow|Carry")
	TObjectPtr<UHollowCarryableDefinition> Definition;

private:
	UFUNCTION()
	void OnRep_CarryState(const FHollowCarryNetState& OldState);

	UFUNCTION()
	void HandleBodySleepEvent(UPrimitiveComponent* SleepingComponent, FName BoneName);

	UFUNCTION()
	void HandleBodyWakeEvent(UPrimitiveComponent* WakingComponent, FName BoneName);

	/** Server: replicate a new state now. */
	void SetCarryState(const FHollowCarryNetState& NewState);
	/** Every machine, once playing: carriers learn the change, the presentation follows, listeners hear it. */
	void HandleStateChanged(const FHollowCarryNetState& OldState);
	/** Every machine: body attachment and collision, visual attachment. IgnoredCarrier is shown as gone. */
	void ApplyPresentation(const APawn* IgnoredCarrier = nullptr);
	/**
	 * Server: where a dropped body fits. The held body passes through the world, so the grip may be inside a wall: try
	 * the grip turned around the carrier (front first) and take the first one where the body fits with nothing
	 * between it and the carrier's centre. Unset if every direction is blocked.
	 */
	TOptional<FVector> FindDropLocation(const APawn& Carrier) const;
	/** The carrier's centre at grip height: inside its capsule, so free once the carrier is gone. */
	FVector GetCarrierCentre(const APawn& Carrier) const;
	/** Clients: move the body onto the last replicated pose (drop start, rest). */
	void SnapToReplicatedPose();
	/** Server: asleep long enough: kinematic, no movement replication, dormant. */
	void SettleAtRest();
	/** Server: awake for MaxAwakeSeconds: sleep now. */
	void ForceSleep();
	void ClearTimers();

	UPROPERTY(ReplicatedUsing = OnRep_CarryState)
	FHollowCarryNetState CarryState;

	TWeakObjectPtr<UPrimitiveComponent> Body;
	TWeakObjectPtr<UPrimitiveComponent> Visual;
	/** The visual's authored offset on the body; kept inside the carrier attach frame too. */
	FTransform VisualRestRelative = FTransform::Identity;
	/** Whether the visual is on a first-person attach component on this machine. */
	bool bShownFirstPerson = false;
	/** Server: dropped and simulating, not settled yet. */
	bool bAwake = false;

	FTimerHandle DormancyTimer;
	FTimerHandle MaxAwakeTimer;
};
