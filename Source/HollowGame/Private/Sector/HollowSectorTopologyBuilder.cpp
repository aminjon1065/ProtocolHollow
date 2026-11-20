#include "Sector/HollowSectorTopologyBuilder.h"

#include "Assembly/HollowSectorManifest.h"
#include "Data/HollowLevelMetrics.h"
#include "Data/HollowRoomDefinition.h"
#include "Generation/HollowSectorGenTypes.h"
#include "Tags/HollowGameplayTags.h"

namespace HollowSectorTopologyBuilderPrivate
{
	EHollowPortalKind PortalKind(const EHollowConnectorKind Kind)
	{
		switch (Kind)
		{
		case EHollowConnectorKind::Door:     return EHollowPortalKind::Door;
		case EHollowConnectorKind::Bulkhead: return EHollowPortalKind::Bulkhead;
		default:                             return EHollowPortalKind::Open;
		}
	}
}

FHollowSectorTopology HollowSectorTopologyBuilder::Build(const FHollowSectorManifest& Manifest, const FHollowSectorLayout& Layout,
	const UHollowLevelMetrics& Metrics, const FVector& Origin)
{
	FHollowSectorTopology Topology;
	if (Manifest.Rooms.Num() != Layout.Rooms.Num())
	{
		return Topology;   // not one sector: nothing the AI could trust
	}
	const double CellSize = Metrics.CellSize;
	for (int32 RoomIndex = 0; RoomIndex < Layout.Rooms.Num(); ++RoomIndex)
	{
		const FHollowPlacedRoom& Placed = Layout.Rooms[RoomIndex];
		const UHollowRoomDefinition* Room = Manifest.Rooms[RoomIndex].Room;
		const int32 Storeys = Room ? FMath::Max(Room->Footprint.Z, 1) : 1;
		const FVector Min = Origin + FVector(Placed.Cell.X * CellSize, Placed.Cell.Y * CellSize, 0.0);
		const FVector Max = Origin + FVector((Placed.Cell.X + Placed.Size.X) * CellSize, (Placed.Cell.Y + Placed.Size.Y) * CellSize,
			static_cast<double>(Storeys) * Metrics.CellHeight);
		Topology.Rooms.Add({FBox(Min, Max)});
		if (!Room)
		{
			continue;
		}
		const FTransform RoomTransform = HollowSectorManifest::RoomTransform(Placed, FIntPoint(Room->Footprint.X, Room->Footprint.Y), Metrics, Origin);
		for (const FHollowRoomSocket& Socket : Room->Sockets)
		{
			if (Socket.SocketType == HollowTags::Socket_VentNode)
			{
				const FVector Location = (Socket.Transform * RoomTransform).GetLocation();
				Topology.Vents.Add({RoomIndex, FVector(Location.X, Location.Y, Min.Z)});   // the AI walks to the floor below the grille
			}
		}
	}
	for (const FHollowPlacedConnection& Connection : Layout.Connections)
	{
		const FVector Location = HollowSectorManifest::OpeningTransform(Connection.CellA, Connection.SideA, Metrics, Origin).GetLocation();
		Topology.Portals.Add({Connection.RoomA, Connection.RoomB, Location, HollowSectorTopologyBuilderPrivate::PortalKind(Connection.Kind)});
	}
	return Topology;
}
