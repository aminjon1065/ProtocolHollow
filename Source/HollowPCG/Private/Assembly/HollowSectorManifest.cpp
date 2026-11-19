#include "Assembly/HollowSectorManifest.h"

#include "Data/HollowBiomeDefinition.h"
#include "Data/HollowLevelMetrics.h"
#include "Data/HollowRoomDefinition.h"
#include "Generation/HollowGridMath.h"
#include "Generation/HollowSectorTemplates.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowSectorManifest)

namespace HollowSectorManifestPrivate
{
	bool FitsByte(const int32 Value)
	{
		return Value >= 0 && Value <= TNumericLimits<uint8>::Max();
	}

	double Yaw(const EHollowRoomSide Side)
	{
		constexpr double QuarterTurn = 90.0;
		return static_cast<int32>(Side) * QuarterTurn;
	}
}

bool HollowSectorManifest::Build(const FHollowSectorLayout& Layout, TConstArrayView<UHollowRoomDefinition*> Definitions, UHollowBiomeDefinition* Biome,
	const FName SectorPlugin, const int32 BuildId, FHollowSectorManifest& OutManifest, FString& OutError)
{
	using HollowSectorManifestPrivate::FitsByte;
	OutManifest = FHollowSectorManifest();
	if (!Biome || Definitions.Num() != Layout.TemplateIds.Num() || BuildId == 0)
	{
		OutError = TEXT("manifest needs the biome, one definition per template id and a non-zero build id");
		return false;
	}

	OutManifest.Biome = Biome;
	OutManifest.SectorPlugin = SectorPlugin;
	OutManifest.Seed = Layout.Seed;
	OutManifest.BuildId = BuildId;
	OutManifest.LayoutHash = Layout.LayoutHash;
	if (!FitsByte(Layout.Rooms.Num()))
	{
		OutError = TEXT("too many rooms for the manifest");
		return false;
	}
	for (const FHollowPlacedRoom& Placed : Layout.Rooms)
	{
		if (!Definitions.IsValidIndex(Placed.Template) || !Definitions[Placed.Template] || !FitsByte(Placed.Cell.X) || !FitsByte(Placed.Cell.Y))
		{
			OutError = TEXT("room without definition or outside the manifest range");
			return false;
		}
		FHollowManifestRoom& Room = OutManifest.Rooms.AddDefaulted_GetRef();
		Room.Room = Definitions[Placed.Template];
		Room.X = static_cast<uint8>(Placed.Cell.X);
		Room.Y = static_cast<uint8>(Placed.Cell.Y);
		Room.Yaw90 = Placed.Yaw90;
	}
	for (const FHollowPlacedConnection& Placed : Layout.Connections)
	{
		if (!FitsByte(Placed.ConnectorA) || !FitsByte(Placed.ConnectorB))
		{
			OutError = TEXT("connector index outside the manifest range");
			return false;
		}
		FHollowManifestConnection& Connection = OutManifest.Connections.AddDefaulted_GetRef();
		Connection.RoomA = static_cast<uint8>(Placed.RoomA);
		Connection.ConnectorA = static_cast<uint8>(Placed.ConnectorA);
		Connection.RoomB = static_cast<uint8>(Placed.RoomB);
		Connection.ConnectorB = static_cast<uint8>(Placed.ConnectorB);
		Connection.Kind = Placed.Kind;
	}
	return true;
}

bool HollowSectorManifest::Reconstruct(const FHollowSectorManifest& Manifest, FHollowSectorLayout& OutLayout, FString& OutError)
{
	using namespace HollowGridMath;
	OutLayout = FHollowSectorLayout();
	if (!Manifest.Biome)
	{
		OutError = TEXT("manifest has no biome");
		return false;
	}
	OutLayout.Seed = Manifest.Seed;
	OutLayout.LayoutHash = Manifest.LayoutHash;

	TArray<const UHollowRoomDefinition*> Definitions;
	for (const FHollowManifestRoom& Room : Manifest.Rooms)
	{
		const UHollowRoomDefinition* Definition = Room.Room;
		const TOptional<EHollowRoomRole> Role = Definition ? HollowSectorTemplates::ResolveRole(Definition->RoomTags, Manifest.Biome->GenRules) : TOptional<EHollowRoomRole>();
		if (!Role.IsSet())
		{
			OutError = FString::Printf(TEXT("room %d is missing or has no role under the biome"), Definitions.Num());
			return false;
		}
		const FString Id = Definition->GetPrimaryAssetId().ToString();
		int32 Template = OutLayout.TemplateIds.IndexOfByKey(Id);
		if (Template == INDEX_NONE)
		{
			Template = OutLayout.TemplateIds.Add(Id);
		}

		FHollowPlacedRoom& Placed = OutLayout.Rooms.AddDefaulted_GetRef();
		Placed.Template = Template;
		Placed.Role = Role.GetValue();
		Placed.Cell = FIntPoint(Room.X, Room.Y);
		Placed.Size = RotateSize(FIntPoint(Definition->Footprint.X, Definition->Footprint.Y), Room.Yaw90);
		Placed.Yaw90 = Room.Yaw90;
		Definitions.Add(Definition);
	}

	TArray<TArray<bool>> Used;
	for (const UHollowRoomDefinition* Definition : Definitions)
	{
		Used.AddDefaulted_GetRef().Init(false, Definition->Connectors.Num());
	}
	auto WorldConnector = [&](const int32 RoomIndex, const int32 ConnectorIndex, FIntPoint& OutCell, EHollowRoomSide& OutSide)
	{
		const UHollowRoomDefinition* Definition = Definitions[RoomIndex];
		const FHollowRoomConnector& Connector = Definition->Connectors[ConnectorIndex];
		const FHollowPlacedRoom& Placed = OutLayout.Rooms[RoomIndex];
		OutCell = Placed.Cell + RotateCell(FIntPoint(Connector.Cell.X, Connector.Cell.Y), FIntPoint(Definition->Footprint.X, Definition->Footprint.Y), Placed.Yaw90);
		OutSide = RotateSide(Connector.Side, Placed.Yaw90);
	};

	for (const FHollowManifestConnection& Connection : Manifest.Connections)
	{
		if (!Definitions.IsValidIndex(Connection.RoomA) || !Definitions.IsValidIndex(Connection.RoomB)
			|| !Used[Connection.RoomA].IsValidIndex(Connection.ConnectorA) || !Used[Connection.RoomB].IsValidIndex(Connection.ConnectorB))
		{
			OutError = TEXT("connection references a missing room or connector");
			return false;
		}
		FHollowPlacedConnection& Placed = OutLayout.Connections.AddDefaulted_GetRef();
		Placed.RoomA = Connection.RoomA;
		Placed.ConnectorA = Connection.ConnectorA;
		Placed.RoomB = Connection.RoomB;
		Placed.ConnectorB = Connection.ConnectorB;
		Placed.Kind = Connection.Kind;
		WorldConnector(Placed.RoomA, Placed.ConnectorA, Placed.CellA, Placed.SideA);
		Used[Placed.RoomA][Placed.ConnectorA] = true;
		Used[Placed.RoomB][Placed.ConnectorB] = true;
	}

	for (int32 RoomIndex = 0; RoomIndex < Definitions.Num(); ++RoomIndex)
	{
		for (int32 ConnectorIndex = 0; ConnectorIndex < Used[RoomIndex].Num(); ++ConnectorIndex)
		{
			if (!Used[RoomIndex][ConnectorIndex])
			{
				FHollowSealedConnector& Sealed = OutLayout.Sealed.AddDefaulted_GetRef();
				Sealed.Room = RoomIndex;
				Sealed.Connector = ConnectorIndex;
				WorldConnector(RoomIndex, ConnectorIndex, Sealed.Cell, Sealed.Side);
			}
		}
	}
	return true;
}

int32 HollowSectorManifest::EstimateNetBytes(const FHollowSectorManifest& Manifest)
{
	// Generous per-field sizes: a NetGUID reference is at most 8 bytes once exported, bytes are bytes.
	constexpr int32 ReferenceBytes = 8;
	constexpr int32 HeaderBytes = ReferenceBytes + ReferenceBytes + sizeof(int32) * 3 + sizeof(uint32);
	constexpr int32 RoomBytes = ReferenceBytes + 3;
	constexpr int32 ConnectionBytes = 5;
	constexpr int32 ArrayHeaderBytes = sizeof(int32);
	return HeaderBytes + 2 * ArrayHeaderBytes + Manifest.Rooms.Num() * RoomBytes + Manifest.Connections.Num() * ConnectionBytes;
}

FTransform HollowSectorManifest::RoomTransform(const FHollowPlacedRoom& Room, const FIntPoint& Footprint, const UHollowLevelMetrics& Metrics, const FVector& Origin)
{
	constexpr double QuarterTurn = 90.0;
	const FIntPoint Pivot = Room.Cell + HollowGridMath::PivotOffset(Footprint, Room.Yaw90);
	const FVector Location = Origin + FVector(Pivot.X * Metrics.CellSize, Pivot.Y * Metrics.CellSize, 0.0);
	return FTransform(FRotator(0.0, Room.Yaw90 * QuarterTurn, 0.0), Location);
}

FTransform HollowSectorManifest::OpeningTransform(const FIntPoint& Cell, const EHollowRoomSide Side, const UHollowLevelMetrics& Metrics, const FVector& Origin)
{
	const FIntPoint Step = HollowGridMath::Offset(Side);
	// Doubled coordinates keep the cell centre and the half step integer.
	const FVector Location = Origin + FVector((2 * Cell.X + 1 + Step.X) * 0.5 * Metrics.CellSize, (2 * Cell.Y + 1 + Step.Y) * 0.5 * Metrics.CellSize, 0.0);
	return FTransform(FRotator(0.0, HollowSectorManifestPrivate::Yaw(Side), 0.0), Location);
}

FTransform HollowSectorManifest::PlugTransform(const FIntPoint& Cell, const EHollowRoomSide Side, const UHollowLevelMetrics& Metrics, const FVector& Origin)
{
	FTransform Transform = OpeningTransform(Cell, Side, Metrics, Origin);
	const FIntPoint Step = HollowGridMath::Offset(Side);
	// Back into the room's own wall (walls sit inside the footprint, LevelMetrics.md), centred on the opening's height.
	Transform.AddToTranslation(FVector(-Step.X * 0.5 * Metrics.WallThickness, -Step.Y * 0.5 * Metrics.WallThickness, 0.5 * Metrics.DoorHeight));
	return Transform;
}

FVector HollowSectorManifest::PlugSize(const UHollowLevelMetrics& Metrics)
{
	return FVector(Metrics.WallThickness, Metrics.DoorWidth, Metrics.DoorHeight);
}
