#include "Generation/HollowSectorGraph.h"

namespace HollowSectorGraphPrivate
{
	/** Sorted, de-duplicated (min, max) pairs without self links. */
	TArray<FIntPoint> DistinctPairs(TConstArrayView<FIntPoint> Links)
	{
		TArray<FIntPoint> Pairs;
		Pairs.Reserve(Links.Num());
		for (const FIntPoint& Link : Links)
		{
			if (Link.X != Link.Y)
			{
				Pairs.Add(FIntPoint(FMath::Min(Link.X, Link.Y), FMath::Max(Link.X, Link.Y)));
			}
		}
		Pairs.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X != B.X ? A.X < B.X : A.Y < B.Y; });
		TArray<FIntPoint> Unique;
		for (const FIntPoint& Pair : Pairs)
		{
			if (Unique.IsEmpty() || Unique.Last() != Pair)
			{
				Unique.Add(Pair);
			}
		}
		return Unique;
	}

	TArray<TArray<int32>> Adjacency(const int32 NumRooms, TConstArrayView<FIntPoint> Links)
	{
		TArray<TArray<int32>> Neighbours;
		Neighbours.SetNum(NumRooms);
		for (const FIntPoint& Pair : DistinctPairs(Links))
		{
			if (Pair.X >= 0 && Pair.Y < NumRooms)
			{
				Neighbours[Pair.X].Add(Pair.Y);
				Neighbours[Pair.Y].Add(Pair.X);
			}
		}
		return Neighbours;
	}

	int32 CountComponents(const TArray<TArray<int32>>& Neighbours)
	{
		TArray<bool> Seen;
		Seen.Init(false, Neighbours.Num());
		int32 Components = 0;
		for (int32 Start = 0; Start < Neighbours.Num(); ++Start)
		{
			if (Seen[Start])
			{
				continue;
			}
			++Components;
			TArray<int32> Stack = {Start};
			Seen[Start] = true;
			while (!Stack.IsEmpty())
			{
				const int32 Room = Stack.Pop(EAllowShrinking::No);
				for (const int32 Next : Neighbours[Room])
				{
					if (!Seen[Next])
					{
						Seen[Next] = true;
						Stack.Add(Next);
					}
				}
			}
		}
		return Components;
	}

	bool Overlaps(const FHollowPlacedRoom& A, const FHollowPlacedRoom& B)
	{
		return A.Cell.X < B.Cell.X + B.Size.X && B.Cell.X < A.Cell.X + A.Size.X
			&& A.Cell.Y < B.Cell.Y + B.Size.Y && B.Cell.Y < A.Cell.Y + A.Size.Y;
	}
}

int32 HollowSectorGraph::ShortestPath(const int32 NumRooms, TConstArrayView<FIntPoint> Links, const int32 From, const int32 To, const int32 Excluded)
{
	if (From < 0 || To < 0 || From >= NumRooms || To >= NumRooms || From == Excluded || To == Excluded)
	{
		return INDEX_NONE;
	}

	const TArray<TArray<int32>> Neighbours = HollowSectorGraphPrivate::Adjacency(NumRooms, Links);
	TArray<int32> Distance;
	Distance.Init(INDEX_NONE, NumRooms);
	Distance[From] = 0;
	TArray<int32> Queue = {From};
	for (int32 Head = 0; Head < Queue.Num(); ++Head)
	{
		const int32 Room = Queue[Head];
		if (Room == To)
		{
			return Distance[Room];
		}
		for (const int32 Next : Neighbours[Room])
		{
			if (Next != Excluded && Distance[Next] == INDEX_NONE)
			{
				Distance[Next] = Distance[Room] + 1;
				Queue.Add(Next);
			}
		}
	}
	return INDEX_NONE;
}

int32 HollowSectorGraph::CountComponents(const int32 NumRooms, TConstArrayView<FIntPoint> Links)
{
	return HollowSectorGraphPrivate::CountComponents(HollowSectorGraphPrivate::Adjacency(NumRooms, Links));
}

int32 HollowSectorGraph::CountLoops(const int32 NumRooms, TConstArrayView<FIntPoint> Links)
{
	const int32 Distinct = HollowSectorGraphPrivate::DistinctPairs(Links).Num();
	return Distinct - NumRooms + HollowSectorGraphPrivate::CountComponents(HollowSectorGraphPrivate::Adjacency(NumRooms, Links));
}

TArray<FIntPoint> HollowSectorGraph::LinkPairs(const FHollowSectorLayout& Layout)
{
	TArray<FIntPoint> Pairs;
	Pairs.Reserve(Layout.Connections.Num());
	for (const FHollowPlacedConnection& Connection : Layout.Connections)
	{
		Pairs.Add(FIntPoint(Connection.RoomA, Connection.RoomB));
	}
	return Pairs;
}

FHollowSectorStats HollowSectorGraph::Analyze(const FHollowSectorLayout& Layout)
{
	using namespace HollowSectorGraphPrivate;

	FHollowSectorStats Stats;
	const int32 NumRooms = Layout.Rooms.Num();
	Stats.Rooms = NumRooms;
	if (NumRooms == 0)
	{
		return Stats;
	}

	int32 Spawn = INDEX_NONE;
	int32 Extraction = INDEX_NONE;
	FIntPoint Min = Layout.Rooms[0].Cell;
	FIntPoint Max = Layout.Rooms[0].Cell + Layout.Rooms[0].Size;
	for (int32 Index = 0; Index < NumRooms; ++Index)
	{
		const FHollowPlacedRoom& Room = Layout.Rooms[Index];
		Stats.SpawnRooms += Room.Role == EHollowRoomRole::Spawn ? 1 : 0;
		Stats.ExtractionRooms += Room.Role == EHollowRoomRole::Extraction ? 1 : 0;
		Stats.CoreVaults += Room.Role == EHollowRoomRole::CoreVault ? 1 : 0;
		Spawn = Room.Role == EHollowRoomRole::Spawn ? Index : Spawn;
		Extraction = Room.Role == EHollowRoomRole::Extraction ? Index : Extraction;
		Min = FIntPoint(FMath::Min(Min.X, Room.Cell.X), FMath::Min(Min.Y, Room.Cell.Y));
		Max = FIntPoint(FMath::Max(Max.X, Room.Cell.X + Room.Size.X), FMath::Max(Max.Y, Room.Cell.Y + Room.Size.Y));
		for (int32 Other = 0; Other < Index; ++Other)
		{
			Stats.bOverlapFree &= !Overlaps(Room, Layout.Rooms[Other]);
		}
	}
	Stats.Extent = Max - Min;

	TArray<FIntPoint> OpenPairs;
	for (const FHollowPlacedConnection& Connection : Layout.Connections)
	{
		const bool bBulkhead = Connection.Kind == EHollowConnectorKind::Bulkhead;
		Stats.Bulkheads += bBulkhead ? 1 : 0;
		if (!bBulkhead)
		{
			OpenPairs.Add(FIntPoint(Connection.RoomA, Connection.RoomB));
		}
	}
	Stats.ComponentsWithoutBulkheads = HollowSectorGraphPrivate::CountComponents(Adjacency(NumRooms, OpenPairs));

	const TArray<FIntPoint> Pairs = LinkPairs(Layout);
	const TArray<TArray<int32>> Neighbours = Adjacency(NumRooms, Pairs);
	Stats.Links = DistinctPairs(Pairs).Num();
	Stats.Components = HollowSectorGraphPrivate::CountComponents(Neighbours);
	Stats.Loops = Stats.Links - NumRooms + Stats.Components;
	if (Stats.SpawnRooms == 1 && Stats.ExtractionRooms == 1)
	{
		Stats.SpawnToExtraction = ShortestPath(NumRooms, Pairs, Spawn, Extraction);
	}

	// Dead ends: strip leaves layer by layer; Spawn and Extraction are leaves by design and stay pinned.
	TArray<int32> Degree;
	Degree.SetNum(NumRooms);
	for (int32 Index = 0; Index < NumRooms; ++Index)
	{
		Degree[Index] = Neighbours[Index].Num();
	}
	TArray<bool> Removed;
	Removed.Init(false, NumRooms);
	for (int32 Layer = 1; ; ++Layer)
	{
		TArray<int32> Leaves;
		for (int32 Index = 0; Index < NumRooms; ++Index)
		{
			if (!Removed[Index] && Degree[Index] <= 1 && Index != Spawn && Index != Extraction)
			{
				Leaves.Add(Index);
			}
		}
		if (Leaves.IsEmpty())
		{
			break;
		}
		Stats.DeadEndDepth = Layer;
		for (const int32 Leaf : Leaves)
		{
			Removed[Leaf] = true;
			for (const int32 Next : Neighbours[Leaf])
			{
				--Degree[Next];
			}
		}
	}
	return Stats;
}
