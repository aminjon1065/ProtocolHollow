#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Interfaces/HollowLightSource.h"
#include "Sector/HollowSectorTopology.h"

namespace HollowSectorTopologyTests
{
	// Test data: four rooms of 1000 x 1000 in a row along X (0-1-2-3), a fifth one apart with no portal.
	constexpr double RoomSize = 1000.0;
	constexpr double RoomHeight = 400.0;
	constexpr int32 RowRooms = 4;
	const FVector Apart(10000.0, 0.0, 0.0);

	FHollowSectorTopology Row()
	{
		FHollowSectorTopology Topology;
		for (int32 Room = 0; Room < RowRooms; ++Room)
		{
			Topology.Rooms.Add({FBox(FVector(Room * RoomSize, 0.0, 0.0), FVector((Room + 1) * RoomSize, RoomSize, RoomHeight))});
			if (Room > 0)
			{
				Topology.Portals.Add({Room - 1, Room, FVector(Room * RoomSize, RoomSize * 0.5, 0.0), EHollowPortalKind::Door});
			}
		}
		Topology.Rooms.Add({FBox(Apart, Apart + FVector(RoomSize, RoomSize, RoomHeight))});
		return Topology;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowSectorTopologyTest, "Hollow.Core.Sector.Topology",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowSectorTopologyTest::RunTest(const FString& Parameters)
{
	// The AI's room graph (ADR-0019): rooms by location, portals between them, neighbours and distances in portals.
	using namespace HollowSectorTopologyTests;
	const FHollowSectorTopology Topology = Row();
	TestFalse(TEXT("A sector is not empty"), Topology.IsEmpty());
	TestTrue(TEXT("The default topology is empty"), FHollowSectorTopology().IsEmpty());
	TestEqual(TEXT("A point in the second room"), Topology.RoomAt(FVector(RoomSize * 1.5, RoomSize * 0.5, RoomHeight * 0.5)), 1);
	TestEqual(TEXT("On a shared wall: the first room in order"), Topology.RoomAt(FVector(RoomSize, RoomSize * 0.5, 0.0)), 0);
	TestEqual(TEXT("Above the ceiling: no room"), Topology.RoomAt(FVector(RoomSize * 1.5, RoomSize * 0.5, RoomHeight * 2.0)), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("The portal of 1 and 2, either way round"), Topology.PortalBetween(2, 1), 1);
	TestEqual(TEXT("No portal between 0 and 2"), Topology.PortalBetween(0, 2), static_cast<int32>(INDEX_NONE));
	TestTrue(TEXT("Neighbours of 1, ascending"), Topology.Neighbours(1) == TArray<int32>({0, 2}));
	TestTrue(TEXT("The room apart has no neighbours"), Topology.Neighbours(RowRooms).IsEmpty());
	TestEqual(TEXT("Same room: 0 portals"), Topology.RoomDistance(2, 2), 0);
	TestEqual(TEXT("End to end: 3 portals"), Topology.RoomDistance(0, RowRooms - 1), RowRooms - 1);
	TestEqual(TEXT("To the room apart: unreachable"), Topology.RoomDistance(0, RowRooms), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("Bad index: unreachable"), Topology.RoomDistance(0, RowRooms + 1), static_cast<int32>(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowLightConeTest, "Hollow.Core.Sector.LightCone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowLightConeTest::RunTest(const FString& Parameters)
{
	// What a lamp lights, before occlusion (ADR-0019): inside the half angle and the range.
	FHollowLightCone Cone;
	Cone.Origin = FVector(0.0, 0.0, 100.0);
	Cone.Direction = FVector::ForwardVector;
	Cone.HalfAngleDegrees = 30.f;
	Cone.Range = 1000.f;
	const double Inside = FMath::Tan(FMath::DegreesToRadians(Cone.HalfAngleDegrees - 5.f));
	const double Outside = FMath::Tan(FMath::DegreesToRadians(Cone.HalfAngleDegrees + 5.f));
	const double Ahead = Cone.Range * 0.5;
	TestTrue(TEXT("Straight ahead"), Cone.Contains(Cone.Origin + FVector(Ahead, 0.0, 0.0)));
	TestTrue(TEXT("Just inside the edge"), Cone.Contains(Cone.Origin + FVector(Ahead, Ahead * Inside, 0.0)));
	TestFalse(TEXT("Just outside the edge"), Cone.Contains(Cone.Origin + FVector(Ahead, Ahead * Outside, 0.0)));
	TestFalse(TEXT("Behind"), Cone.Contains(Cone.Origin - FVector(Ahead, 0.0, 0.0)));
	TestFalse(TEXT("Beyond the range"), Cone.Contains(Cone.Origin + FVector(Cone.Range * 1.01, 0.0, 0.0)));
	TestTrue(TEXT("At the lamp itself"), Cone.Contains(Cone.Origin));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
