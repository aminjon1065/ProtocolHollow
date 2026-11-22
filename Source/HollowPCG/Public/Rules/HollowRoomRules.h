#pragma once

#include "CoreMinimal.h"

#include "HollowRoomRules.generated.h"

class UHollowLevelMetrics;
class UHollowRoomDefinition;

/** Problems of one room definition. Index points into Connectors or Sockets where relevant. */
UENUM()
enum class EHollowRoomIssue : uint8
{
	EmptyFootprint,
	TooManyStoreys,
	NoLevel,
	NoConnectors,
	ConnectorOutsideFootprint,
	ConnectorFacesInside,
	DuplicateConnector,
	ConnectorWithoutKinds,
	ConnectorAboveFloor,
	SocketWithoutType,
	SocketOutsideFootprint,
	RoomTypeCount
};

/** Inconsistent level metrics: the kit cannot be built from them. */
UENUM()
enum class EHollowMetricsIssue : uint8
{
	NonPositiveValue,
	DoorDoesNotFitCell,
	DoorReachesCeiling,
	CorridorTooNarrow,
	VentNotSmallerThanDoor,
	AgentDoesNotFitDoor
};

struct FHollowRoomIssue
{
	EHollowRoomIssue Code = EHollowRoomIssue::EmptyFootprint;
	int32 Index = INDEX_NONE;
};

/** Pure validation rules of the modular room kit (Docs/QA/LevelMetrics.md). Used by tests, Data Validation and the generator. */
namespace HollowRoomRules
{
	/** Every rule a room definition breaks; empty means valid. */
	HOLLOWPCG_API TArray<FHollowRoomIssue> ValidateRoom(const UHollowRoomDefinition& Room, const UHollowLevelMetrics& Metrics);

	/** Every consistency rule the metrics break; empty means the kit can be built. */
	HOLLOWPCG_API TArray<EHollowMetricsIssue> ValidateMetrics(const UHollowLevelMetrics& Metrics);

	/** Human-readable description for Data Validation and logs. */
	HOLLOWPCG_API FText Describe(const FHollowRoomIssue& Issue);
}
