#pragma once

#include "CoreMinimal.h"

#include "HollowSectorTopology.generated.h"

/** What links two rooms (Core's own kinds: AI reads the topology without depending on PCG, ADR-0019). */
UENUM(BlueprintType)
enum class EHollowPortalKind : uint8
{
	Open,
	Door,
	Bulkhead
};

USTRUCT(BlueprintType)
struct FHollowTopologyRoom
{
	GENERATED_BODY()

	/** World bounds of the room's footprint, floor to ceiling. */
	UPROPERTY(BlueprintReadOnly, Category = "Topology")
	FBox Bounds = FBox(ForceInit);
};

USTRUCT(BlueprintType)
struct FHollowTopologyPortal
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Topology")
	int32 RoomA = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Topology")
	int32 RoomB = INDEX_NONE;

	/** Centre of the opening, on the floor. */
	UPROPERTY(BlueprintReadOnly, Category = "Topology")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Topology")
	EHollowPortalKind Kind = EHollowPortalKind::Open;
};

/** A vent opening in a room (a VentNode socket), on the floor below it. */
USTRUCT(BlueprintType)
struct FHollowTopologyVent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Topology")
	int32 Room = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Topology")
	FVector Location = FVector::ZeroVector;
};

/**
 * The assembled sector as the AI sees it (ADR-0019): rooms, the portals between them and the vent openings, in world
 * space, in manifest order. Built by the server from the manifest after assembly (Game) and read by heatmap, vents and
 * director (AI); pure queries, deterministic (arrays only).
 */
USTRUCT(BlueprintType)
struct HOLLOWCORE_API FHollowSectorTopology
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Topology")
	TArray<FHollowTopologyRoom> Rooms;

	UPROPERTY(BlueprintReadOnly, Category = "Topology")
	TArray<FHollowTopologyPortal> Portals;

	UPROPERTY(BlueprintReadOnly, Category = "Topology")
	TArray<FHollowTopologyVent> Vents;

	bool IsEmpty() const { return Rooms.IsEmpty(); }

	/** Room whose bounds hold Location (first in manifest order), or INDEX_NONE. */
	int32 RoomAt(const FVector& Location) const;

	/** Portal linking two rooms (first in order), or INDEX_NONE. */
	int32 PortalBetween(int32 RoomA, int32 RoomB) const;

	/** Rooms one portal away, ascending. */
	TArray<int32> Neighbours(int32 Room) const;

	/** Fewest portals between two rooms (0 for the same room), INDEX_NONE if none link them. */
	int32 RoomDistance(int32 From, int32 To) const;
};
