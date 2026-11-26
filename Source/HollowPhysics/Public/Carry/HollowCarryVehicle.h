#pragma once

#include "UObject/Interface.h"

#include "HollowCarryVehicle.generated.h"

class APawn;
class USceneComponent;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UHollowCarryVehicle : public UInterface
{
	GENERATED_BODY()
};

/**
 * DRONE-01 (ADR-0054): what holds a load for one player in EHollowCarryMode::Drone (the solo drone). The load hangs
 * from its carry point on every machine; the player stays the load's carrier (the altar, the extraction). C++ only.
 */
class HOLLOWPHYSICS_API IHollowCarryVehicle
{
	GENERATED_BODY()

public:
	/** Where the load hangs (null: it cannot carry now). */
	virtual USceneComponent* GetCarryPoint() const = 0;

	/** The player it carries for; only that one hands it a load or takes it back. */
	virtual APawn* GetCarryOwner() const = 0;
};
