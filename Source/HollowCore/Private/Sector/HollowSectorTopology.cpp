#include "Sector/HollowSectorTopology.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowSectorTopology)

int32 FHollowSectorTopology::RoomAt(const FVector& Location) const
{
	return Rooms.IndexOfByPredicate([&Location](const FHollowTopologyRoom& Room) { return Room.Bounds.IsInsideOrOn(Location); });
}

int32 FHollowSectorTopology::PortalBetween(const int32 RoomA, const int32 RoomB) const
{
	return Portals.IndexOfByPredicate([RoomA, RoomB](const FHollowTopologyPortal& Portal)
	{
		return (Portal.RoomA == RoomA && Portal.RoomB == RoomB) || (Portal.RoomA == RoomB && Portal.RoomB == RoomA);
	});
}

TArray<int32> FHollowSectorTopology::Neighbours(const int32 Room) const
{
	TArray<int32> Result;
	for (const FHollowTopologyPortal& Portal : Portals)
	{
		if (Portal.RoomA == Room)
		{
			Result.AddUnique(Portal.RoomB);
		}
		else if (Portal.RoomB == Room)
		{
			Result.AddUnique(Portal.RoomA);
		}
	}
	Result.Sort();
	return Result;
}

int32 FHollowSectorTopology::RoomDistance(const int32 From, const int32 To) const
{
	if (!Rooms.IsValidIndex(From) || !Rooms.IsValidIndex(To))
	{
		return INDEX_NONE;
	}
	TArray<int32> Distance;
	Distance.Init(INDEX_NONE, Rooms.Num());
	Distance[From] = 0;
	TArray<int32> Frontier = {From};
	for (int32 Next = 0; Next < Frontier.Num(); ++Next)
	{
		const int32 Room = Frontier[Next];
		if (Room == To)
		{
			return Distance[Room];
		}
		for (const int32 Neighbour : Neighbours(Room))
		{
			if (Distance.IsValidIndex(Neighbour) && Distance[Neighbour] == INDEX_NONE)
			{
				Distance[Neighbour] = Distance[Room] + 1;
				Frontier.Add(Neighbour);
			}
		}
	}
	return INDEX_NONE;
}
