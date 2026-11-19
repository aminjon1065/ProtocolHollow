#pragma once

#include "Data/HollowRoomTypes.h"
#include "GameplayTagContainer.h"
#include "Math/IntPoint.h"

#include "HollowSectorGenTypes.generated.h"

/** What the generator does with a room template; resolved from its Hollow.Room.Type.* tag by FHollowSectorGenRules. */
UENUM()
enum class EHollowRoomRole : uint8
{
	Spawn,
	Extraction,
	CoreVault,
	/** Critical path, branch connectors and loop bridges. */
	Path,
	/** End room of an optional side branch. */
	Branch
};

UENUM()
enum class EHollowGenResult : uint8
{
	Success,
	InvalidInput,
	ExhaustedAttempts
};

/** Why one generation attempt was discarded. */
UENUM()
enum class EHollowGenFailure : uint8
{
	None,
	CriticalPath,
	CoreVaultBranch,
	Loops,
	OptionalBranch,
	BulkheadQuota,
	Validation
};

/**
 * Tuning of the sector generator, stored per biome (UHollowBiomeDefinition). Counts are rooms; a route length is the
 * number of links (connections) between rooms. Defaults are zero on purpose: every value is data.
 */
USTRUCT(BlueprintType)
struct HOLLOWPCG_API FHollowSectorGenRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Roles", meta = (Categories = "Hollow.Room.Type"))
	FGameplayTag SpawnRoomType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Roles", meta = (Categories = "Hollow.Room.Type"))
	FGameplayTag ExtractionRoomType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Roles", meta = (Categories = "Hollow.Room.Type"))
	FGameplayTag CoreVaultRoomType;

	/** Room types that make up the critical path, branch connectors and loop bridges. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Roles", meta = (Categories = "Hollow.Room.Type"))
	FGameplayTagContainer PathRoomTypes;

	/** Room types that end optional side branches. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Roles", meta = (Categories = "Hollow.Room.Type"))
	FGameplayTagContainer BranchRoomTypes;

	/** Rooms strictly between Spawn and Extraction on the critical path. Loops never shorten the route below Min + 1 links. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "1"))
	int32 CriticalPathMin = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "1"))
	int32 CriticalPathMax = 0;

	/** Core vaults, each at the end of its own side branch off the critical path. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "0"))
	int32 CoreVaultCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "0"))
	int32 OptionalBranchesMin = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "0"))
	int32 OptionalBranchesMax = 0;

	/** Rooms in one side branch, its end room included; also the deepest allowed dead end. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "1"))
	int32 MaxBranchDepth = 0;

	/** Independent cycles (links - rooms + 1): every loop is a second way around the Echo-Stalker (CLAUDE.md sec. 5). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "0"))
	int32 MinLoops = 0;

	/** Rooms a loop bridge may insert between two free connectors. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "1"))
	int32 MaxBridgeRooms = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "0"))
	int32 MinBulkheadConnections = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limits", meta = (ClampMin = "2"))
	int32 MaxRooms = 0;

	/** Edge of the sector's bounding box, in cells. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limits", meta = (ClampMin = "1"))
	int32 MaxExtentCells = 0;

	/** Attempts with derived seeds before the generator gives up (ADR-0003). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limits", meta = (ClampMin = "1"))
	int32 MaxAttempts = 0;

	/** Search budget of one attempt: room placements tried, including the ones undone by backtracking. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limits", meta = (ClampMin = "1"))
	int32 MaxPlacementsPerAttempt = 0;
};

/** Connector of a room template in room-local cells (FHollowRoomConnector without the storey). */
struct FHollowTemplateConnector
{
	FIntPoint Cell = FIntPoint::ZeroValue;
	EHollowRoomSide Side = EHollowRoomSide::North;
	uint8 Kinds = 0;
};

/** Plain snapshot of a UHollowRoomDefinition for the generator (no UObject access during generation). */
struct FHollowRoomTemplate
{
	/** PrimaryAssetId string; the generator orders its pool by it (CLAUDE.md sec. 5). */
	FString Id;
	EHollowRoomRole Role = EHollowRoomRole::Path;
	FIntPoint Footprint = FIntPoint(1, 1);
	TArray<FHollowTemplateConnector> Connectors;
};

struct FHollowPlacedRoom
{
	/** Index into FHollowSectorLayout::TemplateIds. */
	int32 Template = INDEX_NONE;
	EHollowRoomRole Role = EHollowRoomRole::Path;
	/** Min corner of the rotated footprint (layout cells start at 0,0). */
	FIntPoint Cell = FIntPoint::ZeroValue;
	/** Rotated footprint. */
	FIntPoint Size = FIntPoint(1, 1);
	uint8 Yaw90 = 0;
	bool bCriticalPath = false;

	bool operator==(const FHollowPlacedRoom&) const = default;
};

/** Two connectors joined; the door, bulkhead or opening sits on the boundary between CellA and its neighbour across SideA. */
struct FHollowPlacedConnection
{
	int32 RoomA = INDEX_NONE;
	int32 ConnectorA = INDEX_NONE;
	int32 RoomB = INDEX_NONE;
	int32 ConnectorB = INDEX_NONE;
	FIntPoint CellA = FIntPoint::ZeroValue;
	EHollowRoomSide SideA = EHollowRoomSide::North;
	EHollowConnectorKind Kind = EHollowConnectorKind::None;

	bool operator==(const FHollowPlacedConnection&) const = default;
};

/** A connector left unused: the assembly plugs its opening (S2-02). */
struct FHollowSealedConnector
{
	int32 Room = INDEX_NONE;
	int32 Connector = INDEX_NONE;
	FIntPoint Cell = FIntPoint::ZeroValue;
	EHollowRoomSide Side = EHollowRoomSide::North;

	bool operator==(const FHollowSealedConnector&) const = default;
};

/** Result of the generator: integer grid layout of one sector. */
struct FHollowSectorLayout
{
	int32 Seed = 0;
	/** Attempts used, the successful one included. */
	int32 Attempts = 0;
	/** CRC32 of the canonical layout (rooms, cells, rotations, connections); compared by clients (ADR-0003). */
	uint32 LayoutHash = 0;
	/** Ids of the template pool in generator order (sorted); FHollowPlacedRoom::Template indexes it. */
	TArray<FString> TemplateIds;
	TArray<FHollowPlacedRoom> Rooms;
	TArray<FHollowPlacedConnection> Connections;
	TArray<FHollowSealedConnector> Sealed;

	bool operator==(const FHollowSectorLayout&) const = default;
};

struct FHollowSectorGenInput
{
	int32 Seed = 0;
	FHollowSectorGenRules Rules;
	/** Any order: the generator sorts by Id. */
	TArray<FHollowRoomTemplate> Templates;
};

/** Why generation failed, for logs and tests. */
struct FHollowSectorGenReport
{
	TArray<FString> InputErrors;
	TArray<EHollowGenFailure> AttemptFailures;
};
