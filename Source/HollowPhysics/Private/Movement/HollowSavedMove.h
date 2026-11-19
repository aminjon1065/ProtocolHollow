#pragma once

#include "GameFramework/CharacterMovementComponent.h"

/** Saved move carrying the Hollow movement requests (see UHollowCharacterMovementComponent, HollowMoveFlags). */
class FSavedMove_Hollow : public FSavedMove_Character
{
public:
	using Super = FSavedMove_Character;

	virtual void Clear() override;
	virtual void SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;
	virtual uint8 GetCompressedFlags() const override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override;

private:
	bool bSavedWantsToSprint = false;
};

/** Client prediction data that allocates FSavedMove_Hollow. */
class FNetworkPredictionData_Client_Hollow : public FNetworkPredictionData_Client_Character
{
public:
	using Super = FNetworkPredictionData_Client_Character;

	explicit FNetworkPredictionData_Client_Hollow(const UCharacterMovementComponent& ClientMovement);

	virtual FSavedMovePtr AllocateNewMove() override;
};
