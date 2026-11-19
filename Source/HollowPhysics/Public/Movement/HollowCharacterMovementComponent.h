#pragma once

#include "GameFramework/CharacterMovementComponent.h"

#include "HollowCharacterMovementComponent.generated.h"

class UHollowMovementTuning;

/** Compressed move flags used by Hollow movement: four custom bits are available (ADR-0002). */
namespace HollowMoveFlags
{
	/** Sprint request of the move. */
	inline constexpr uint8 Sprint = FSavedMove_Character::FLAG_Custom_0;
	// FLAG_Custom_1 / FLAG_Custom_2: heavy-carry mode (S3-02). FLAG_Custom_3: spare.
}

/**
 * Character movement of Protocol: Hollow. Every request that changes movement speed travels inside the client's
 * saved moves (custom compressed flags), so client prediction and server simulation agree without corrections
 * (CLAUDE.md sec. 3, ADR-0002). Tuning numbers come from UHollowMovementTuning, never from code.
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

	/** True while the sprint request is honoured (on the ground, see HollowMovementMath::CanSprint). */
	bool IsSprinting() const;

	/** Applies a tuning asset immediately (e.g. when an ExoFrame class is selected). */
	void SetMovementTuning(const UHollowMovementTuning* InTuning);
	const UHollowMovementTuning* GetMovementTuning() const { return Tuning; }

	virtual void InitializeComponent() override;
	virtual float GetMaxSpeed() const override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

protected:
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual bool ClientUpdatePositionAfterServerUpdate() override;

private:
	void ApplyTuning(const UHollowMovementTuning& InTuning);

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Movement")
	TObjectPtr<const UHollowMovementTuning> Tuning;

	/** Local request on the owning client; on the server, the request decoded from the move being simulated. */
	bool bWantsToSprint = false;

	friend class FHollowSavedMoveSprintFlagTest;   // automation test driving the server-side flag decode
};
