#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "Carry/HollowCarryTypes.h"
#include "Components/ActorComponent.h"
#include "Templates/SubclassOf.h"

#include "HollowCarrierComponent.generated.h"

class AController;
class APawn;
class UAbilitySystemComponent;
class UHollowCarryableComponent;
class UHollowCharacterMovementComponent;
class UGameplayEffect;
class USceneComponent;

/**
 * The carrier's side of heavy carrying (ADR-0015), on the player pawn. The carried object's state is the only truth
 * (UHollowCarryableComponent); this component follows it on every machine:
 * - Owning client: the Interact input on a free carryable predicts the pick-up, i.e. starts claiming the slowdown in
 *   its saved moves before the server answers (a refusal clears it after PickupPredictionTimeoutSeconds). The drop is
 *   not predicted: the claim stays until the server's free state arrives, so the server never sees a faster move.
 * - Server: applies the carryable's solo effect to the carrier's ability system and the server carry state to the
 *   movement component; drops the object when the carrier goes away.
 * Solo and listen-host players use the same path (their server calls run locally). No Tick.
 *
 * Co-op (ADR-0028): Interact on an object someone carries alone predicts joining (claims Coop); carrying with a
 * partner, this pawn's movement is tethered to it, and Interact, death or leaving only lets go of the other's carry.
 */
UCLASS(ClassGroup = (Hollow), meta = (BlueprintSpawnableComponent))
class HOLLOWPHYSICS_API UHollowCarrierComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnHollowCarriedChanged, const UHollowCarrierComponent& /*Carrier*/);

	explicit UHollowCarrierComponent(const FObjectInitializer& ObjectInitializer);

	/** Every machine: what this pawn carries, or how (Solo, Coop, Drone), changed (the solo drone's focus, DRONE-01). */
	FOnHollowCarriedChanged OnCarriedChanged;

	/** Components a carried visual attaches to: the first-person view (camera) and the third-person body mesh. */
	void SetAttachTargets(USceneComponent* FirstPerson, USceneComponent* ThirdPerson);
	USceneComponent* GetAttachTarget(bool bFirstPerson) const;

	/** What this pawn carries according to the replicated (or server) state. */
	UHollowCarryableComponent* GetCarried() const { return Carried.Get(); }
	bool IsCarrying() const { return Carried.IsValid(); }

	/** A predicted pick-up or join waiting for the server (owning client), and the claim it predicts. */
	UHollowCarryableComponent* GetPendingPickup() const { return PendingPickup.Get(); }
	EHollowCarryClaim GetPendingClaim() const { return PendingPickup.IsValid() ? PendingClaim : EHollowCarryClaim::None; }

	/** The other carrier while two carry, or null. */
	APawn* GetPartner() const;

	/**
	 * Local Interact input. Carrying: asks the server to drop (or let go of a co-op carry) and returns true (input
	 * consumed); while the own drone carries it (DRONE-01) the hands are free, so returns false. Focusing a free carryable or one someone carries alone: predicts the pick-up or the join and returns
	 * false, so the interaction goes on to the server's checks.
	 */
	bool HandleInteractInput(AActor* Focus);

	/** Server: lift Carryable alone (the carryable owner's Interact, after the interaction checks). */
	bool ServerPickUp(UHollowCarryableComponent* Carryable);

	/** Server: take the other side of what someone carries alone (Solo -> Coop, ADR-0028). */
	bool ServerJoin(UHollowCarryableComponent* Carryable);

	/**
	 * Server: stop carrying. Carried by two: let go, the partner carries on alone. Alone: put it down, with part of the
	 * velocity, where it fits; without room around the carrier it stays carried, unless bForce (the carrier is going away).
	 */
	void ServerDrop(bool bForce = false);

	/** Every machine: the carryable's state now says this pawn carries it (or no longer does). */
	void NotifyCarryChanged(UHollowCarryableComponent* Carryable, bool bCarrying);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION(Server, Reliable)
	void Server_Drop();

	UFUNCTION()
	void HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);

	/** Locally controlled pawns (owning client, listen host, AI) claim what they carry or predict. */
	void RefreshClaim();
	void HandlePredictionTimeout();
	void ApplyCarryEffect(const UHollowCarryableComponent& Carryable);
	void RemoveCarryEffect();

	APawn* GetPawn() const;
	UHollowCharacterMovementComponent* GetMovement() const;

	TWeakObjectPtr<UHollowCarryableComponent> Carried;
	TWeakObjectPtr<UHollowCarryableComponent> PendingPickup;
	EHollowCarryClaim PendingClaim = EHollowCarryClaim::None;
	TWeakObjectPtr<USceneComponent> FirstPersonTarget;
	TWeakObjectPtr<USceneComponent> ThirdPersonTarget;
	FTimerHandle PredictionTimer;
	/** Server: the carryable's solo or co-op effect on this carrier, and the ability system it lives on (on the player
	 *  state, so it outlives this pawn: the removal must not depend on the pawn still being its avatar). */
	FActiveGameplayEffectHandle CarryEffect;
	TSubclassOf<UGameplayEffect> CarryEffectClass;
	TWeakObjectPtr<UAbilitySystemComponent> CarryEffectOwner;
};
