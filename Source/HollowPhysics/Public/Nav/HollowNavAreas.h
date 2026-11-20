#pragma once

#include "NavAreas/NavArea.h"

#include "HollowNavAreas.generated.h"

/**
 * Areas of a door's nav link (ADR-0004, Docs/Architecture/03 sec. 3.7). Only types live here: travel costs and
 * exclusions are data in the query filters (NAV_Filter_Standard, NAV_Filter_Heavy), so tuning needs no C++ rebuild.
 */
UCLASS(Abstract)
class HOLLOWPHYSICS_API UHollowNavArea_Door : public UNavArea
{
	GENERATED_BODY()
};

/** Open, opening or breached: a plain doorway. */
UCLASS()
class HOLLOWPHYSICS_API UHollowNavArea_Doorway : public UHollowNavArea_Door
{
	GENERATED_BODY()

public:
	explicit UHollowNavArea_Doorway(const FObjectInitializer& ObjectInitializer);
};

/** Closed (or closing): the AI opens it. */
UCLASS()
class HOLLOWPHYSICS_API UHollowNavArea_DoorClosed : public UHollowNavArea_Door
{
	GENERATED_BODY()

public:
	explicit UHollowNavArea_DoorClosed(const FObjectInitializer& ObjectInitializer);
};

UCLASS()
class HOLLOWPHYSICS_API UHollowNavArea_DoorLocked : public UHollowNavArea_Door
{
	GENERATED_BODY()

public:
	explicit UHollowNavArea_DoorLocked(const FObjectInitializer& ObjectInitializer);
};

/** Welded below UHollowDoorDefinition::WeldedHighThreshold. */
UCLASS()
class HOLLOWPHYSICS_API UHollowNavArea_DoorWeldedLow : public UHollowNavArea_Door
{
	GENERATED_BODY()

public:
	explicit UHollowNavArea_DoorWeldedLow(const FObjectInitializer& ObjectInitializer);
};

UCLASS()
class HOLLOWPHYSICS_API UHollowNavArea_DoorWeldedHigh : public UHollowNavArea_Door
{
	GENERATED_BODY()

public:
	explicit UHollowNavArea_DoorWeldedHigh(const FObjectInitializer& ObjectInitializer);
};

UCLASS()
class HOLLOWPHYSICS_API UHollowNavArea_DoorBarricaded : public UHollowNavArea_Door
{
	GENERATED_BODY()

public:
	explicit UHollowNavArea_DoorBarricaded(const FObjectInitializer& ObjectInitializer);
};

/**
 * A bulkhead whose passage is still shut (Docs/Architecture/03 sec. 3.7): Standard agents are excluded, breachers
 * (Heavy) may plan through it and break it. On breach the link switches to the doorway area.
 */
UCLASS()
class HOLLOWPHYSICS_API UHollowNavArea_BulkheadIntact : public UNavArea
{
	GENERATED_BODY()

public:
	explicit UHollowNavArea_BulkheadIntact(const FObjectInitializer& ObjectInitializer);
};
