#pragma once

#include "UObject/Interface.h"

#include "HollowItemCostSource.generated.h"

class AActor;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UHollowItemCostSource : public UInterface
{
	GENERATED_BODY()
};

/**
 * What holds the items abilities cost (ADR-0030): the raid inventory of a player (Economy implements it, GAS only
 * asks). An item is the object that granted the ability (its spec's source object, an item definition). Read on the
 * server and on the owning client (its count replicates to the owner); only the server takes.
 */
class HOLLOWGAS_API IHollowItemCostSource
{
	GENERATED_BODY()

public:
	/** How many of Item the owner holds (0 for anything it does not know). */
	virtual int32 GetItemCount(const UObject* Item) const = 0;

	/** Server: takes Count of Item. False, and nothing taken, if fewer are held. */
	virtual bool ConsumeItem(const UObject* Item, int32 Count) = 0;

	/** The source on Owner (the ability system's owner actor, a player state) or one of its components; null if none. */
	static IHollowItemCostSource* Find(const AActor* Owner);
};
