#include "Rules/HollowRoomRules.h"

#include "Data/HollowLevelMetrics.h"
#include "Data/HollowRoomDefinition.h"
#include "Tags/HollowGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowRoomRules)

#define LOCTEXT_NAMESPACE "HollowRoomRules"

namespace HollowRoomRulesPrivate
{
	bool IsInsideFootprint(const FIntVector& Cell, const FIntVector& Footprint)
	{
		return Cell.X >= 0 && Cell.Y >= 0 && Cell.Z >= 0 && Cell.X < Footprint.X && Cell.Y < Footprint.Y && Cell.Z < Footprint.Z;
	}

	/** A connector must lie on the outer boundary of the footprint, facing out. */
	bool FacesOutside(const FIntVector& Cell, const EHollowRoomSide Side, const FIntVector& Footprint)
	{
		switch (Side)
		{
		case EHollowRoomSide::North: return Cell.X == Footprint.X - 1;
		case EHollowRoomSide::South: return Cell.X == 0;
		case EHollowRoomSide::East:  return Cell.Y == Footprint.Y - 1;
		case EHollowRoomSide::West:  return Cell.Y == 0;
		}
		return false;
	}

	/** Strict child of Parent (the parent tag itself is not a valid value). */
	bool IsChildOf(const FGameplayTag& Tag, const FGameplayTag& Parent)
	{
		return Tag.IsValid() && Tag != Parent && Tag.MatchesTag(Parent);
	}
}

TArray<FHollowRoomIssue> HollowRoomRules::ValidateRoom(const UHollowRoomDefinition& Room, const UHollowLevelMetrics& Metrics)
{
	using namespace HollowRoomRulesPrivate;
	TArray<FHollowRoomIssue> Issues;
	auto Add = [&Issues](const EHollowRoomIssue Code, const int32 Index = INDEX_NONE) { Issues.Add({Code, Index}); };

	const FIntVector& Footprint = Room.Footprint;
	// Placement rules are meaningless without a footprint: report only the footprint then.
	const bool bHasFootprint = Footprint.X > 0 && Footprint.Y > 0 && Footprint.Z > 0;
	if (!bHasFootprint)
	{
		Add(EHollowRoomIssue::EmptyFootprint);
	}
	else if (Footprint.Z != 1)
	{
		Add(EHollowRoomIssue::MultiStorey);   // vertical slice: one storey (LevelMetrics.md)
	}
	if (Room.RoomLevel.IsNull())
	{
		Add(EHollowRoomIssue::NoLevel);
	}

	if (Room.Connectors.IsEmpty())
	{
		Add(EHollowRoomIssue::NoConnectors);
	}
	for (int32 Index = 0; Index < Room.Connectors.Num(); ++Index)
	{
		const FHollowRoomConnector& Connector = Room.Connectors[Index];
		if (bHasFootprint)
		{
			if (!IsInsideFootprint(Connector.Cell, Footprint))
			{
				Add(EHollowRoomIssue::ConnectorOutsideFootprint, Index);
			}
			else if (!FacesOutside(Connector.Cell, Connector.Side, Footprint))
			{
				Add(EHollowRoomIssue::ConnectorFacesInside, Index);
			}
		}
		if (Connector.AllowedKinds == 0)
		{
			Add(EHollowRoomIssue::ConnectorWithoutKinds, Index);
		}
		for (int32 Earlier = 0; Earlier < Index; ++Earlier)
		{
			if (Room.Connectors[Earlier].Cell == Connector.Cell && Room.Connectors[Earlier].Side == Connector.Side)
			{
				Add(EHollowRoomIssue::DuplicateConnector, Index);
				break;
			}
		}
	}

	const FVector Extent(Footprint.X * Metrics.CellSize, Footprint.Y * Metrics.CellSize, Footprint.Z * Metrics.CellHeight);
	const FBox RoomBox(FVector::ZeroVector, Extent);
	for (int32 Index = 0; Index < Room.Sockets.Num(); ++Index)
	{
		const FHollowRoomSocket& Socket = Room.Sockets[Index];
		if (!IsChildOf(Socket.SocketType, HollowTags::Socket))
		{
			Add(EHollowRoomIssue::SocketWithoutType, Index);
		}
		if (bHasFootprint && !RoomBox.IsInsideOrOn(Socket.Transform.GetLocation()))
		{
			Add(EHollowRoomIssue::SocketOutsideFootprint, Index);
		}
	}

	int32 RoomTypes = 0;
	for (const FGameplayTag& Tag : Room.RoomTags)
	{
		RoomTypes += IsChildOf(Tag, HollowTags::Room_Type) ? 1 : 0;
	}
	if (RoomTypes != 1)
	{
		Add(EHollowRoomIssue::RoomTypeCount);
	}
	return Issues;
}

TArray<EHollowMetricsIssue> HollowRoomRules::ValidateMetrics(const UHollowLevelMetrics& M)
{
	TArray<EHollowMetricsIssue> Issues;
	const int32 Values[] = {M.CellSize, M.CellHeight, M.WallThickness, M.SlabThickness, M.DoorWidth, M.DoorHeight,
		M.MinCorridorWidth, M.VentSize, M.LargestAgentRadius, M.LargestAgentHeight};
	for (const int32 Value : Values)
	{
		if (Value <= 0)
		{
			Issues.Add(EHollowMetricsIssue::NonPositiveValue);
			return Issues;   // the remaining rules are meaningless on missing values
		}
	}

	// Jambs at least one wall thick: the perpendicular wall at a room corner must not eat into the opening.
	if (M.DoorWidth + 2 * M.WallThickness > M.CellSize)
	{
		Issues.Add(EHollowMetricsIssue::DoorDoesNotFitCell);
	}
	if (M.DoorHeight >= M.CellHeight)
	{
		Issues.Add(EHollowMetricsIssue::DoorReachesCeiling);
	}
	// A one-cell corridor has a wall on both sides.
	if (M.CellSize - 2 * M.WallThickness < M.MinCorridorWidth)
	{
		Issues.Add(EHollowMetricsIssue::CorridorTooNarrow);
	}
	if (M.VentSize >= M.DoorWidth || M.VentSize >= M.DoorHeight)
	{
		Issues.Add(EHollowMetricsIssue::VentNotSmallerThanDoor);
	}
	if (2 * M.LargestAgentRadius > M.DoorWidth || M.LargestAgentHeight > M.DoorHeight || 2 * M.LargestAgentRadius > M.MinCorridorWidth)
	{
		Issues.Add(EHollowMetricsIssue::AgentDoesNotFitDoor);
	}
	return Issues;
}

FText HollowRoomRules::Describe(const FHollowRoomIssue& Issue)
{
	FText What;
	switch (Issue.Code)
	{
	case EHollowRoomIssue::EmptyFootprint:            What = LOCTEXT("EmptyFootprint", "Footprint must be at least one cell on every axis."); break;
	case EHollowRoomIssue::MultiStorey:               What = LOCTEXT("MultiStorey", "Footprint.Z must be 1: the vertical slice has one storey."); break;
	case EHollowRoomIssue::NoLevel:                   What = LOCTEXT("NoLevel", "RoomLevel is not set."); break;
	case EHollowRoomIssue::NoConnectors:              What = LOCTEXT("NoConnectors", "A room needs at least one connector."); break;
	case EHollowRoomIssue::ConnectorOutsideFootprint: What = LOCTEXT("ConnectorOutside", "Connector cell is outside the footprint."); break;
	case EHollowRoomIssue::ConnectorFacesInside:      What = LOCTEXT("ConnectorInside", "Connector does not lie on the outer boundary (its side faces another cell of the room)."); break;
	case EHollowRoomIssue::DuplicateConnector:        What = LOCTEXT("DuplicateConnector", "Two connectors share the same cell and side."); break;
	case EHollowRoomIssue::ConnectorWithoutKinds:     What = LOCTEXT("ConnectorWithoutKinds", "Connector allows no kind (Door, Bulkhead or Open)."); break;
	case EHollowRoomIssue::SocketWithoutType:         What = LOCTEXT("SocketWithoutType", "Socket type must be a Hollow.Socket.* tag."); break;
	case EHollowRoomIssue::SocketOutsideFootprint:    What = LOCTEXT("SocketOutside", "Socket lies outside the room footprint."); break;
	case EHollowRoomIssue::RoomTypeCount:             What = LOCTEXT("RoomTypeCount", "RoomTags must contain exactly one Hollow.Room.Type.* tag."); break;
	}
	return Issue.Index == INDEX_NONE ? What : FText::Format(LOCTEXT("WithIndex", "[{0}] {1}"), FText::AsNumber(Issue.Index), What);
}

#undef LOCTEXT_NAMESPACE
