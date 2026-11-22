#pragma once

#include "Attributes/HollowAttributeSetBase.h"

#include "HollowMovementSet.generated.h"

/**
 * Movement of an enemy (S6-02, ADR-0035): a multiplier of the speed its brain asks for. Effects slow it (the Aether
 * lantern's GE_Occult_Expose, the ritual ward of S6-03); the brain applies it to every move. Neutral (1) unless an
 * effect changes it. Server only: enemies move on the server, clients see the replicated movement, so it is not
 * replicated.
 */
UCLASS()
class HOLLOWGAS_API UHollowMovementSet : public UHollowAttributeSetBase
{
	GENERATED_BODY()

public:
	UHollowMovementSet();

	ATTRIBUTE_ACCESSORS_BASIC(UHollowMovementSet, MoveSpeedMultiplier);

private:
	/** Multiplier of the speed the brain asks for. */
	UPROPERTY(BlueprintReadOnly, Category = "Hollow|Movement", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MoveSpeedMultiplier;
};
