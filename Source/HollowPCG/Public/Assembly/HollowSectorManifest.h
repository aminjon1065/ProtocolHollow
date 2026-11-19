#pragma once

#include "Generation/HollowSectorGenTypes.h"
#include "Math/Transform.h"

#include "HollowSectorManifest.generated.h"

class UHollowBiomeDefinition;
class UHollowLevelMetrics;
class UHollowRoomDefinition;

USTRUCT()
struct HOLLOWPCG_API FHollowManifestRoom
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UHollowRoomDefinition> Room;

	UPROPERTY()
	uint8 X = 0;

	UPROPERTY()
	uint8 Y = 0;

	UPROPERTY()
	uint8 Yaw90 = 0;
};

USTRUCT()
struct HOLLOWPCG_API FHollowManifestConnection
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 RoomA = 0;

	UPROPERTY()
	uint8 ConnectorA = 0;

	UPROPERTY()
	uint8 RoomB = 0;

	UPROPERTY()
	uint8 ConnectorB = 0;

	UPROPERTY()
	EHollowConnectorKind Kind = EHollowConnectorKind::None;
};

/**
 * What the server replicates for a sector (ADR-0003): rooms, cells, rotations and connections, a few hundred bytes.
 * Every machine rebuilds the full layout from it with its local room definitions and compares LayoutHash.
 */
USTRUCT()
struct HOLLOWPCG_API FHollowSectorManifest
{
	GENERATED_BODY()

	/** Its rules give the room roles that LayoutHash covers. */
	UPROPERTY()
	TObjectPtr<UHollowBiomeDefinition> Biome;

	/** Sector Game Feature to activate on every machine (ADR-0012). */
	UPROPERTY()
	FName SectorPlugin;

	UPROPERTY()
	int32 Seed = 0;

	/** Server counter per assembly; 0 means no sector. Keeps level instance names unique across rebuilds. */
	UPROPERTY()
	int32 BuildId = 0;

	UPROPERTY()
	uint32 LayoutHash = 0;

	UPROPERTY()
	TArray<FHollowManifestRoom> Rooms;

	UPROPERTY()
	TArray<FHollowManifestConnection> Connections;

	bool IsSet() const { return BuildId != 0; }
};

namespace HollowSectorManifest
{
	/** Server: generator layout -> manifest. Definitions[i] is the room of Layout.TemplateIds[i]. */
	HOLLOWPCG_API bool Build(const FHollowSectorLayout& Layout, TConstArrayView<UHollowRoomDefinition*> Definitions, UHollowBiomeDefinition* Biome,
		FName SectorPlugin, int32 BuildId, FHollowSectorManifest& OutManifest, FString& OutError);

	/** Every machine: manifest -> layout from the local room definitions and the biome's roles (hash it to compare). */
	HOLLOWPCG_API bool Reconstruct(const FHollowSectorManifest& Manifest, FHollowSectorLayout& OutLayout, FString& OutError);

	/** Upper bound of the replicated size in bytes (object references counted as full NetGUIDs). */
	HOLLOWPCG_API int32 EstimateNetBytes(const FHollowSectorManifest& Manifest);

	/** Room level instance: room-local frame (origin at its footprint's -X/-Y corner on the floor) -> world. */
	HOLLOWPCG_API FTransform RoomTransform(const FHollowPlacedRoom& Room, const FIntPoint& Footprint, const UHollowLevelMetrics& Metrics, const FVector& Origin);

	/** Centre of the opening between Cell and its neighbour across Side, on the floor; X axis points across the opening. */
	HOLLOWPCG_API FTransform OpeningTransform(const FIntPoint& Cell, EHollowRoomSide Side, const UHollowLevelMetrics& Metrics, const FVector& Origin);

	/** Centre of the plug that fills a sealed opening inside its own room's wall; X axis points across the wall. */
	HOLLOWPCG_API FTransform PlugTransform(const FIntPoint& Cell, EHollowRoomSide Side, const UHollowLevelMetrics& Metrics, const FVector& Origin);

	/** Plug box: wall thickness x door width x door height. */
	HOLLOWPCG_API FVector PlugSize(const UHollowLevelMetrics& Metrics);
}
