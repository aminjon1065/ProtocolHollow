#pragma once

#include "Carry/HollowCarryTypes.h"
#include "Components/ActorComponent.h"

#include "HollowCarryableComponent.generated.h"

class APawn;
class UHollowCarryableDefinition;
class UPrimitiveComponent;
class USceneComponent;

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
	ThirdPerson,
	/** Carried by two (ADR-0028): a world object between the carriers as this machine renders them. */
	Coop
};

/**
 * A heavy object players carry (Docs/Architecture/03 sec. 3.5, ADR-0015). The server owns FHollowCarryNetState
 * (push model; the owner is dormant except while a dropped body is awake). While carried, the body rides on the
 * carrier's capsule at GripOffset (the server attaches it, the engine's attachment replication repeats it) and every
 * machine attaches the visual mesh locally: the carrier's own view to its first-person component, everyone else to
 * the carrier's smoothed body. Dropped: the server simulates the body (Predictive Interpolation); once asleep it turns
 * kinematic, stops replicating movement and goes dormant, and clients settle on the replicated rest pose.
 * The carrier's side (effects, movement claim) is UHollowCarrierComponent; no Tick.
 *
 * Co-op (ADR-0028): a second player joins the solo carry (Solo -> Coop). The object then sits between the two
 * (HollowCarryCoop::Pose): its body from their capsules (the server truth), its visual from the bodies this machine
 * renders, recomputed whenever either carrier moves (their meshes' TransformUpdated, no Tick). The server checks the
 * client-authoritative carriers every CoopSanityIntervalSeconds; a leave, a broken tether or a refused carrier leaves
 * the other one carrying alone (Coop -> Solo), and the last one out drops it.
 */
UCLASS(ClassGroup = (Hollow), meta = (BlueprintSpawnableComponent))
class HOLLOWPHYSICS_API UHollowCarryableComponent : public UActorComponent
{
	GENERATED_BODY()

	/** Server, co-op: a carrier's position for the sanity check, when, and its client timestamp then (remote carriers). */
	struct FSanitySample
	{
		FVector Location = FVector::ZeroVector;
		double Time = 0.0;
		double ClientTime = 0.0;
	};

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
	 * Whether Pawn could take the other side now (local focus and server check): someone else carries this alone, Pawn
	 * holds nothing, and they stand no further apart than the tether allows.
	 */
	bool CanJoin(const APawn* Pawn) const;

	/** Server: Secondary joins the solo carrier (Solo -> Coop); both carriers learn it through the state change. */
	bool BeginCoop(APawn* Secondary);

	/**
	 * Server: one of two carriers lets go; the other carries on alone (Coop -> Solo, it becomes the primary). False if
	 * this is not carried by two with Carrier among them (a solo carrier puts it down with Release).
	 */
	bool ReleaseCarrier(APawn* Carrier, EHollowCarryLeave Reason);

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

	/** A carrier's body moved (co-op): the pose follows. */
	void HandleCarrierMoved(USceneComponent* Component, EUpdateTransformFlags Flags, ETeleportType Teleport);

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

	/** Co-op: the pose between the two carriers, the body's from their capsules and the visual's as rendered here. */
	void UpdateCoopPose();
	void BindCoopPose(APawn& Primary, APawn& Secondary);
	void UnbindCoopPose();
	/** Server, co-op: the sanity check of the carriers (speed, inside the world, tether), every CoopSanityIntervalSeconds. */
	void CheckCoopSanity();
	/** Server: starts or stops the sanity check with the mode (fresh samples at the start). */
	void UpdateSanityTimer();
	/** Server: where a carrier is now for the sanity check: a client-authoritative one's newest accepted claim. */
	FSanitySample SampleOf(const APawn& Carrier, double Now) const;
	/** Server: whether a carrier capsule at Location is inside the world, or got there through it from From. */
	bool IsThroughWorld(const APawn& Carrier, const FVector& From, const FVector& Location) const;

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
	/** Whether the visual is between two carriers on this machine. */
	bool bShownCoop = false;

	FTimerHandle DormancyTimer;
	FTimerHandle MaxAwakeTimer;
	FTimerHandle SanityTimer;

	/** Co-op: the carriers' components whose moves move the pose (their meshes), and the bindings. */
	TWeakObjectPtr<USceneComponent> PoseAnchors[2];
	FDelegateHandle PoseHandles[2];

	/** Server, co-op: where each carrier (primary, secondary) last passed the sanity check, and when. */
	FSanitySample SanitySamples[2];
};
