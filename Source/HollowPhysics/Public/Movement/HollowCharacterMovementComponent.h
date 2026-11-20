#pragma once

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

	/** Carry claim of the move (EHollowCarryClaim, two bits: FLAG_Custom_1 low, FLAG_Custom_2 high; ADR-0015). */
	inline constexpr uint8 CarryLow = FSavedMove_Character::FLAG_Custom_1;
	inline constexpr uint8 CarryMask = FSavedMove_Character::FLAG_Custom_1 | FSavedMove_Character::FLAG_Custom_2;
	static_assert(FSavedMove_Character::FLAG_Custom_2 == FSavedMove_Character::FLAG_Custom_1 << 1, "carry claim bits must be adjacent");
	// FLAG_Custom_3: spare (a weight class of the carried object, if cores ever differ).

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

	/** Applies a tuning asset immediately (e.g. when an ExoFrame class is selected). */
	void SetMovementTuning(const UHollowMovementTuning* InTuning);
	const UHollowMovementTuning* GetMovementTuning() const { return Tuning; }

	virtual void InitializeComponent() override;
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxAcceleration() const override;
	virtual bool CanAttemptJump() const override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

protected:
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual bool ClientUpdatePositionAfterServerUpdate() override;

private:
	void ApplyTuning(const UHollowMovementTuning& InTuning);
	/** Carry tuning of the effective slowdown; null while not carrying (or without tuning). */
	const FHollowCarryModeTuning* GetCarryTuning() const;
	/** Server, remote carrier: is the move being simulated with the server's floor instead of its claim? */
	bool IsCarryFloorEnforced() const;

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

	friend class FHollowSavedMoveSprintFlagTest;   // automation tests driving the server-side flag decode
	friend class FHollowSavedMoveCarryBitsTest;
};
