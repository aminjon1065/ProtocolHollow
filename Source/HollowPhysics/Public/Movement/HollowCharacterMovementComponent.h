#pragma once

#include "Carry/HollowCarryCoop.h"
#include "Carry/HollowCarryPolicy.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "HollowCharacterMovementComponent.generated.h"

class UHollowMovementTuning;
struct FHollowCarryModeTuning;

/** Compressed move flags used by Hollow movement: four custom bits are available (ADR-0002). */
namespace HollowMoveFlags
{
	/** Sprint request of the move. */
	inline constexpr uint8 Sprint = FSavedMove_Character::FLAG_Custom_0;

	/**
	 * Carry claim of the move (EHollowCarryClaim, three bits: FLAG_Custom_1 low to FLAG_Custom_3 high; ADR-0015). DRONE-01
	 * (ADR-0054) took the third, once spare, for the drone's claim: no custom bit is left.
	 */
	inline constexpr uint8 CarryLow = FSavedMove_Character::FLAG_Custom_1;
	inline constexpr uint8 CarryMask = FSavedMove_Character::FLAG_Custom_1 | FSavedMove_Character::FLAG_Custom_2 | FSavedMove_Character::FLAG_Custom_3;
	static_assert(FSavedMove_Character::FLAG_Custom_2 == FSavedMove_Character::FLAG_Custom_1 << 1
		&& FSavedMove_Character::FLAG_Custom_3 == FSavedMove_Character::FLAG_Custom_2 << 1, "carry claim bits must be adjacent");
	static_assert(static_cast<uint8>(EHollowCarryClaim::Drone) * CarryLow <= CarryMask, "every carry claim fits its bits");

	inline uint8 EncodeCarry(const EHollowCarryClaim Claim) { return static_cast<uint8>((static_cast<uint8>(Claim) * CarryLow) & CarryMask); }
	inline EHollowCarryClaim DecodeCarry(const uint8 Flags) { return static_cast<EHollowCarryClaim>((Flags & CarryMask) / CarryLow); }
}

/**
 * Character movement of Protocol: Hollow. Every request that changes movement speed travels inside the client's
 * saved moves (custom compressed flags), so client prediction and server simulation agree without corrections
 * (CLAUDE.md sec. 3, ADR-0002). Tuning numbers come from UHollowMovementTuning, never from code.
 *
 * Heavy carrying (ADR-0015): the locally controlled carrier claims its carry slowdown in every move; the server
 * simulates a remote carrier with HollowCarryPolicy::Effective(claim, server state), so speed, acceleration, sprint
 * and jump agree on both sides whenever the client claims what the server knows.
 *
 * Co-op carrying (ADR-0028): a carrier is tethered to its partner (HollowCarryTether) by whoever controls it. The
 * partner is an interpolated proxy there, so the server could never agree: a remote co-op carrier's position comes
 * from its client meanwhile (client-authoritative, checked by the carried object's sanity timer), and after the carry
 * the server takes over again only once the client's own moves agree with its simulation (FHollowCoopReturn).
 */
UCLASS()
class HOLLOWPHYSICS_API UHollowCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	explicit UHollowCharacterMovementComponent(const FObjectInitializer& ObjectInitializer);

	/** Sprint request from local input (or AI); predicted and replicated through FLAG_Custom_0. */
	void SetWantsToSprint(bool bInWantsToSprint) { bWantsToSprint = bInWantsToSprint; }
	bool WantsToSprint() const { return bWantsToSprint; }

	/** True while the sprint request is honoured (on the ground, carry allows it, see HollowMovementMath::CanSprint). */
	bool IsSprinting() const;

	/**
	 * Carry slowdown this machine's control claims (owning client, listen host, AI): set by UHollowCarrierComponent,
	 * sent in every saved move. On the server, for a remote carrier, it is the claim of the move being simulated.
	 */
	void SetClaimedCarry(EHollowCarryClaim Claim) { ClaimedCarry = Claim; }
	EHollowCarryClaim GetClaimedCarry() const { return ClaimedCarry; }

	/** Server only: what the server knows this character carries (UHollowCarrierComponent); the change time starts the grace. */
	void SetServerCarry(EHollowCarryClaim Claim);
	const FHollowServerCarry& GetServerCarry() const { return ServerCarry; }

	/** The slowdown movement uses now: the policy for a remote carrier on the server, otherwise the claim. */
	EHollowCarryClaim GetEffectiveCarry() const;

	/**
	 * Co-op carry, every machine: the partner this character is tethered to while two carry (null: no longer). On the
	 * server a remote carrier turns client-authoritative with a partner and starts the way back without one.
	 */
	void SetCoopPartner(const APawn* Partner, const FHollowCoopCarryTuning* Tether);
	const APawn* GetCoopPartner() const { return CoopPartner.Get(); }

	/** Server: whether this remote carrier's position comes from its client now (co-op carry, or on the way back). */
	bool IsClientAuthoritative() const { return bServerAcceptClientAuthoritativePosition; }

	/** Server: a carrier that left a co-op carry and is still client-authoritative. */
	bool IsReturningAuthority() const { return CoopReturn.IsSet(); }

	/** Server: the sanity check refused this carrier: server-authoritative at once, back at SaneLocation (corrected). */
	void ForceServerAuthority(const FVector& SaneLocation);

	/** Server: the client timestamp of the newest move of this remote character (0 before any). */
	double GetClientTimeStamp() const;

	/**
	 * Server, client-authoritative: the newest position the server took from the client (a checked move) and that move's
	 * client timestamp. Between checked moves the server simulates the character itself (without the client's tether),
	 * so only these are the client's own claims. False before the first one since it turned client-authoritative.
	 */
	bool GetAcceptedClientMove(FVector& OutLocation, double& OutTimeStamp) const;

	/** Walking speed (no sprint) of a carry claim: what a carrier may move at, for the server's sanity check. */
	float GetCarryWalkSpeed(EHollowCarryClaim Claim) const;

	/** Corrections this machine received for its own movement since it spawned (dev statistics, Hollow.Carry.Dump). */
	int32 GetCorrectionCount() const { return CorrectionCount; }

	/** Applies a tuning asset immediately (e.g. when an ExoFrame class is selected). */
	void SetMovementTuning(const UHollowMovementTuning* InTuning);
	const UHollowMovementTuning* GetMovementTuning() const { return Tuning; }

	virtual void InitializeComponent() override;
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxAcceleration() const override;
	virtual bool CanAttemptJump() const override;
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

protected:
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual bool ClientUpdatePositionAfterServerUpdate() override;
	virtual void MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags, const FVector& NewAccel) override;
	/** Owning client: a frame longer than MaxMoveDeltaTime goes out as several moves (HollowMovementMath::MoveSteps). */
	virtual void ReplicateMoveToServer(float DeltaTime, const FVector& NewAcceleration) override;
	virtual void ServerMove_PerformMovement(const FCharacterNetworkMoveData& MoveData) override;

private:
	void ApplyTuning(const UHollowMovementTuning& InTuning);
	/** Carry tuning of the effective slowdown; null while not carrying (or without tuning). */
	const FHollowCarryModeTuning* GetCarryTuning() const;
	/** Server, remote carrier: is the move being simulated with the server's floor instead of its claim? */
	bool IsCarryFloorEnforced() const;
	/** Server: a character another machine controls (its moves arrive as server moves). */
	bool IsRemoteOnServer() const;
	void SetClientAuthority(bool bClientAuthoritative);
	/** Server, after a checked move of a carrier on the way back: server-authoritative again once FHollowCoopReturn allows. */
	void UpdateCoopReturn(bool bInSync);

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Movement")
	TObjectPtr<const UHollowMovementTuning> Tuning;

	/** Local request on the owning client; on the server, the request decoded from the move being simulated. */
	bool bWantsToSprint = false;

	/** Local claim; on the server, for a remote carrier, decoded from the move being simulated. */
	EHollowCarryClaim ClaimedCarry = EHollowCarryClaim::None;

	/** Server: carry state from gameplay and when it changed. */
	FHollowServerCarry ServerCarry;

	/** Server: whether the last simulated move of a remote carrier ran on the floor instead of its claim (logged on change). */
	bool bCarryFloorEnforced = false;

	/** Co-op carry: the partner and the carried object's tether (every machine). */
	TWeakObjectPtr<const APawn> CoopPartner;
	FHollowCoopCarryTuning CoopTether;

	/** Server: the way back to server authority after a co-op carry (set while it lasts). */
	TOptional<FHollowCoopReturn> CoopReturn;
	/** Server: the client claimed Coop since this carrier turned client-authoritative (only then can it show it left). */
	bool bSawCoopClaim = false;

	/** Server, client-authoritative: the newest client position taken and its move's client timestamp. */
	FVector AcceptedClientLocation = FVector::ZeroVector;
	double AcceptedClientTimeStamp = 0.0;
	bool bHasAcceptedClientMove = false;

	/** Server: where this character's own simulation of the last server move ended, before the client's position was taken. */
	FVector SimulatedLocation = FVector::ZeroVector;
	bool bHasSimulatedLocation = false;

	int32 CorrectionCount = 0;

	friend class FHollowSavedMoveSprintFlagTest;   // automation tests driving the server-side flag decode
	friend class FHollowSavedMoveCarryBitsTest;
	friend class FHollowCoopAuthorityTest;
};
