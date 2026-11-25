#pragma once

#include "UObject/Interface.h"

#include "HollowImpulseReceiver.generated.h"

class AActor;

/** One physical push (GP-01, ADR-0044): a shot's pellets on one target, a melee blow. */
USTRUCT(BlueprintType)
struct FHollowImpulse
{
	GENERATED_BODY()

	/** Velocity change before the receiver's own resistance (cm/s, along the push). */
	UPROPERTY(BlueprintReadWrite, Category = "Impulse")
	FVector Impulse = FVector::ZeroVector;

	/** Where it lands (world). */
	UPROPERTY(BlueprintReadWrite, Category = "Impulse")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Impulse")
	TWeakObjectPtr<AActor> Instigator;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UHollowImpulseReceiver : public UInterface
{
	GENERATED_BODY()
};

/**
 * Something a shot or a blow pushes (GP-01): enemies stagger back, loose props fly. Server only: what moves replicates
 * the way it always does (an enemy's movement, a prop's physics), nothing is sent for the push itself.
 */
class HOLLOWCORE_API IHollowImpulseReceiver
{
	GENERATED_BODY()

public:
	/** Server only. */
	virtual void ReceiveImpulse(const FHollowImpulse& Impulse) = 0;
};
