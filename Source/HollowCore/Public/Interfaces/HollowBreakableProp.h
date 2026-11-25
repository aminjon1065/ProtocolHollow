#pragma once

#include "Interfaces/HollowStructural.h"
#include "UObject/Interface.h"

#include "HollowBreakableProp.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UHollowBreakableProp : public UInterface
{
	GENERATED_BODY()
};

/**
 * A prop that shots and blows break (GP-01b: loot crates), unlike doors and bulkheads, which only a breach opens: the
 * weapons and the melee blow hit these, never an IHollowStructural. Server only; what it does replicates as its state.
 */
class HOLLOWCORE_API IHollowBreakableProp
{
	GENERATED_BODY()

public:
	/** Server only. True if the hit changed anything. */
	virtual bool ApplyPropHit(const FHollowStructuralHit& Hit) = 0;
};
