#pragma once

#include "CoreMinimal.h"
#include "Sector/HollowSectorTopology.h"

class UHollowLevelMetrics;
struct FHollowSectorLayout;
struct FHollowSectorManifest;

namespace HollowSectorTopologyBuilder
{
	/**
	 * The AI's view of an assembled sector (ADR-0019), pure: room footprints as world boxes (floor to CellHeight), a
	 * portal per placed connection at its opening, a vent per VentNode socket, all in manifest order.
	 */
	HOLLOWGAME_API FHollowSectorTopology Build(const FHollowSectorManifest& Manifest, const FHollowSectorLayout& Layout, const UHollowLevelMetrics& Metrics,
		const FVector& Origin);
}
