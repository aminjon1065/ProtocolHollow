#pragma once

#include "Generation/HollowSectorGenTypes.h"

/** Topology of a finished layout: what the generator checks before it accepts an attempt. */
struct FHollowSectorStats
{
	int32 Rooms = 0;
	/** Distinct linked room pairs (two doors between the same rooms count once). */
	int32 Links = 0;
	int32 Components = 0;
	/** Links - Rooms + Components. */
	int32 Loops = 0;
	int32 SpawnRooms = 0;
	int32 ExtractionRooms = 0;
	int32 CoreVaults = 0;
	int32 Bulkheads = 0;
	/** Shortest Spawn-to-Extraction route in links; INDEX_NONE without exactly one of each or without a route. */
	int32 SpawnToExtraction = INDEX_NONE;
	/** Longest chain of rooms hanging off the rest of the sector (Spawn and Extraction excluded). */
	int32 DeadEndDepth = 0;
	FIntPoint Extent = FIntPoint::ZeroValue;
	bool bOverlapFree = true;
};

/** Pure graph algorithms on room indices; deterministic (arrays only, no hash container iteration). */
namespace HollowSectorGraph
{
	/** Shortest route in links (INDEX_NONE if none); Excluded is treated as a missing room. */
	HOLLOWPCG_API int32 ShortestPath(int32 NumRooms, TConstArrayView<FIntPoint> Links, int32 From, int32 To, int32 Excluded = INDEX_NONE);

	/** Independent cycles of the simple graph: distinct linked pairs - rooms + components. */
	HOLLOWPCG_API int32 CountLoops(int32 NumRooms, TConstArrayView<FIntPoint> Links);

	/** Room pairs of the layout's connections. */
	HOLLOWPCG_API TArray<FIntPoint> LinkPairs(const FHollowSectorLayout& Layout);

	HOLLOWPCG_API FHollowSectorStats Analyze(const FHollowSectorLayout& Layout);
}
