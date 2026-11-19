#include "Generation/HollowSectorGenerator.h"

#include "Algo/AnyOf.h"
#include "Algo/NoneOf.h"
#include "Containers/StringConv.h"
#include "Generation/HollowGridMath.h"
#include "Generation/HollowRandom.h"
#include "Generation/HollowSectorGraph.h"
#include "HollowPCGLog.h"
#include "Misc/Crc.h"
#include "Util/HollowEnumUtils.h"

namespace HollowSectorGenPrivate
{
	using namespace HollowGridMath;

	constexpr int32 RoleCount = static_cast<int32>(EHollowRoomRole::Branch) + 1;

	int32 RoleIndex(const EHollowRoomRole Role)
	{
		return static_cast<int32>(Role);
	}

	uint8 KindBit(const EHollowConnectorKind Kind)
	{
		return static_cast<uint8>(Kind);
	}

	struct FWorldConnector
	{
		FIntPoint Cell = FIntPoint::ZeroValue;
		EHollowRoomSide Side = EHollowRoomSide::North;
		uint8 Kinds = 0;
		/** Index into FAttempt::Links, INDEX_NONE while free. */
		int32 Link = INDEX_NONE;
	};

	struct FRoomState
	{
		int32 Template = INDEX_NONE;
		FIntPoint Cell = FIntPoint::ZeroValue;
		FIntPoint Size = FIntPoint(1, 1);
		uint8 Yaw90 = 0;
		bool bCritical = false;
		TArray<FWorldConnector> Connectors;
	};

	struct FLink
	{
		int32 RoomA = INDEX_NONE;
		int32 ConnectorA = INDEX_NONE;
		int32 RoomB = INDEX_NONE;
		int32 ConnectorB = INDEX_NONE;
		uint8 Kinds = 0;
		EHollowConnectorKind Kind = EHollowConnectorKind::None;
	};

	/** A way to attach a template to a free connector. */
	struct FCandidate
	{
		int32 Template = INDEX_NONE;
		uint8 Yaw90 = 0;
		int32 Connector = INDEX_NONE;
		FIntPoint Cell = FIntPoint::ZeroValue;
	};

	struct FFreeConnector
	{
		int32 Room = INDEX_NONE;
		int32 Connector = INDEX_NONE;
	};

	/** One generation attempt. Rooms and links form stacks, so every search step can be undone exactly. */
	class FAttempt
	{
	public:
		FAttempt(const TArray<FHollowRoomTemplate>& InTemplates, const FHollowSectorGenRules& InRules, const uint64 AttemptSeed)
			: Templates(InTemplates)
			, Rules(InRules)
			, Rng(AttemptSeed)
		{
			for (int32 Index = 0; Index < Templates.Num(); ++Index)
			{
				ByRole[RoleIndex(Templates[Index].Role)].Add(Index);
			}
			for (const int32 Index : ByRole[RoleIndex(EHollowRoomRole::Path)])
			{
				MaxPathSpan = FMath::Max(MaxPathSpan, FMath::Max(Templates[Index].Footprint.X, Templates[Index].Footprint.Y));
			}
		}

		EHollowGenFailure Run()
		{
			if (!BuildCriticalPath())
			{
				return EHollowGenFailure::CriticalPath;
			}
			if (!BuildCoreVaults())
			{
				return EHollowGenFailure::CoreVaultBranch;
			}
			if (!BuildLoops())
			{
				return EHollowGenFailure::Loops;
			}
			if (!BuildOptionalBranches())
			{
				return EHollowGenFailure::OptionalBranch;
			}
			if (!AssignKinds())
			{
				return EHollowGenFailure::BulkheadQuota;
			}
			return EHollowGenFailure::None;
		}

		void Export(FHollowSectorLayout& Out) const
		{
			Out.TemplateIds.Reset();
			for (const FHollowRoomTemplate& Template : Templates)
			{
				Out.TemplateIds.Add(Template.Id);
			}
			for (const FRoomState& Room : Rooms)
			{
				FHollowPlacedRoom& Placed = Out.Rooms.AddDefaulted_GetRef();
				Placed.Template = Room.Template;
				Placed.Role = Templates[Room.Template].Role;
				Placed.Cell = Room.Cell - BoundsMin;
				Placed.Size = Room.Size;
				Placed.Yaw90 = Room.Yaw90;
				Placed.bCriticalPath = Room.bCritical;
			}
			for (const FLink& Link : Links)
			{
				const FWorldConnector& From = Rooms[Link.RoomA].Connectors[Link.ConnectorA];
				FHollowPlacedConnection& Connection = Out.Connections.AddDefaulted_GetRef();
				Connection.RoomA = Link.RoomA;
				Connection.ConnectorA = Link.ConnectorA;
				Connection.RoomB = Link.RoomB;
				Connection.ConnectorB = Link.ConnectorB;
				Connection.CellA = From.Cell - BoundsMin;
				Connection.SideA = From.Side;
				Connection.Kind = Link.Kind;
			}
			for (int32 RoomIndex = 0; RoomIndex < Rooms.Num(); ++RoomIndex)
			{
				for (int32 ConnectorIndex = 0; ConnectorIndex < Rooms[RoomIndex].Connectors.Num(); ++ConnectorIndex)
				{
					const FWorldConnector& Connector = Rooms[RoomIndex].Connectors[ConnectorIndex];
					if (Connector.Link == INDEX_NONE)
					{
						FHollowSealedConnector& Sealed = Out.Sealed.AddDefaulted_GetRef();
						Sealed.Room = RoomIndex;
						Sealed.Connector = ConnectorIndex;
						Sealed.Cell = Connector.Cell - BoundsMin;
						Sealed.Side = Connector.Side;
					}
				}
			}
		}

	private:
		// ------------------------------------------------------------------ phases

		bool BuildCriticalPath()
		{
			const TArray<int32>& Spawns = ByRole[RoleIndex(EHollowRoomRole::Spawn)];
			FCandidate Start;
			Start.Template = Spawns[static_cast<int32>(Rng.Below(static_cast<uint32>(Spawns.Num())))];
			Start.Yaw90 = static_cast<uint8>(Rng.Below(static_cast<uint32>(SideCount)));
			SpawnRoom = Place(Start, INDEX_NONE, INDEX_NONE, /*bCritical=*/true);

			const int32 PathRooms = Rng.RangeInclusive(Rules.CriticalPathMin, Rules.CriticalPathMax);
			if (!GrowChain(SpawnRoom, PathRooms, EHollowRoomRole::Extraction, /*bCritical=*/true))
			{
				return false;
			}
			ExtractionRoom = Rooms.Num() - 1;
			for (int32 Index = SpawnRoom + 1; Index < ExtractionRoom; ++Index)
			{
				CriticalInterior.Add(Index);
			}
			return true;
		}

		bool BuildCoreVaults()
		{
			for (int32 Vault = 0; Vault < Rules.CoreVaultCount; ++Vault)
			{
				if (!GrowBranch(EHollowRoomRole::CoreVault))
				{
					return false;
				}
			}
			return true;
		}

		bool BuildLoops()
		{
			ConnectAdjacent();
			while (CountLoops() < Rules.MinLoops)
			{
				if (!AddBridge())
				{
					return false;
				}
				ConnectAdjacent();
			}
			return true;
		}

		bool BuildOptionalBranches()
		{
			if (Rules.OptionalBranchesMax <= 0 || ByRole[RoleIndex(EHollowRoomRole::Branch)].IsEmpty())
			{
				return Rules.OptionalBranchesMin <= 0;
			}
			const int32 Count = Rng.RangeInclusive(Rules.OptionalBranchesMin, Rules.OptionalBranchesMax);
			for (int32 Branch = 0; Branch < Count; ++Branch)
			{
				if (!GrowBranch(EHollowRoomRole::Branch))
				{
					return Branch >= Rules.OptionalBranchesMin;   // the optional rest is skipped when the sector is full
				}
			}
			return true;
		}

		/** Bulkhead quota first (random among links that allow it), then Door, Open, Bulkhead by preference. */
		bool AssignKinds()
		{
			const uint8 Bulkhead = KindBit(EHollowConnectorKind::Bulkhead);
			TArray<int32> BulkheadCapable;
			for (int32 Index = 0; Index < Links.Num(); ++Index)
			{
				if ((Links[Index].Kinds & Bulkhead) != 0)
				{
					BulkheadCapable.Add(Index);
				}
			}
			if (BulkheadCapable.Num() < Rules.MinBulkheadConnections)
			{
				return false;
			}
			Rng.Shuffle(BulkheadCapable);
			for (int32 Pick = 0; Pick < Rules.MinBulkheadConnections; ++Pick)
			{
				Links[BulkheadCapable[Pick]].Kind = EHollowConnectorKind::Bulkhead;
			}
			for (FLink& Link : Links)
			{
				if (Link.Kind != EHollowConnectorKind::None)
				{
					continue;
				}
				for (const EHollowConnectorKind Preferred : {EHollowConnectorKind::Door, EHollowConnectorKind::Open, EHollowConnectorKind::Bulkhead})
				{
					if ((Link.Kinds & KindBit(Preferred)) != 0)
					{
						Link.Kind = Preferred;
						break;
					}
				}
			}
			return true;
		}

		// ------------------------------------------------------------------ search

		bool OutOfBudget() const
		{
			return Placements >= Rules.MaxPlacementsPerAttempt;
		}

		/** Grows PathRooms Path rooms from FromRoom and ends with one EndRole room. Undoes everything on failure. */
		bool GrowChain(const int32 FromRoom, const int32 PathRooms, const EHollowRoomRole EndRole, const bool bCritical)
		{
			if (Rooms.Num() >= Rules.MaxRooms || OutOfBudget())
			{
				return false;
			}
			const EHollowRoomRole Role = PathRooms > 0 ? EHollowRoomRole::Path : EndRole;
			TArray<int32> Free = FreeConnectors(FromRoom);
			Rng.Shuffle(Free);
			for (const int32 Connector : Free)
			{
				TArray<FCandidate> Candidates;
				CollectCandidates(FromRoom, Connector, Role, Candidates);
				for (const FCandidate& Candidate : Candidates)
				{
					if (OutOfBudget())
					{
						return false;
					}
					const int32 NewRoom = Place(Candidate, FromRoom, Connector, bCritical);
					if (PathRooms == 0 || GrowChain(NewRoom, PathRooms - 1, EndRole, bCritical))
					{
						return true;
					}
					PopRoom();
				}
			}
			return false;
		}

		/** A side branch off the critical path ending with EndRole; tries the drawn depth first, then shorter ones. */
		bool GrowBranch(const EHollowRoomRole EndRole)
		{
			const int32 DrawnDepth = Rng.RangeInclusive(1, Rules.MaxBranchDepth);
			TArray<int32> Starts = CriticalInterior;
			Rng.Shuffle(Starts);
			for (int32 Depth = DrawnDepth; Depth >= 1; --Depth)
			{
				for (const int32 Start : Starts)
				{
					if (GrowChain(Start, Depth - 1, EndRole, /*bCritical=*/false))
					{
						return true;
					}
					if (OutOfBudget())
					{
						return false;
					}
				}
			}
			return false;
		}

		/** Links free connectors that already face each other, unless the rooms are linked or the route would get too short. */
		void ConnectAdjacent()
		{
			for (int32 RoomIndex = 0; RoomIndex < Rooms.Num(); ++RoomIndex)
			{
				for (int32 ConnectorIndex = 0; ConnectorIndex < Rooms[RoomIndex].Connectors.Num(); ++ConnectorIndex)
				{
					const FWorldConnector& Connector = Rooms[RoomIndex].Connectors[ConnectorIndex];
					if (Connector.Link != INDEX_NONE)
					{
						continue;
					}
					const FFreeConnector Facing = FindFacing(Connector);
					if (Facing.Room == INDEX_NONE || Facing.Room == RoomIndex || AreLinked(RoomIndex, Facing.Room))
					{
						continue;
					}
					AddLink(RoomIndex, ConnectorIndex, Facing.Room, Facing.Connector);
					if (!KeepsRouteLength())
					{
						UnlinkLast();
					}
				}
			}
		}

		/**
		 * Closes one loop: from a free connector, grows up to MaxBridgeRooms Path rooms until one of them faces a free
		 * connector of another room. Starts closest to a possible target go first.
		 */
		bool AddBridge()
		{
			struct FStart
			{
				FFreeConnector From;
				int32 Distance = 0;
			};
			TArray<FStart> Starts;
			for (const FFreeConnector& From : UsableFreeConnectors())
			{
				const int32 Distance = NearestTarget(OutsideCell(From), From.Room);
				if (Distance <= Reach(Rules.MaxBridgeRooms))
				{
					Starts.Add({From, Distance});
				}
			}
			Rng.Shuffle(Starts);
			Starts.StableSort([](const FStart& L, const FStart& R) { return L.Distance < R.Distance; });

			for (const FStart& Start : Starts)
			{
				if (Bridge(Start.From, Rules.MaxBridgeRooms))
				{
					return true;
				}
				if (OutOfBudget())
				{
					return false;
				}
			}
			return false;
		}

		/** Upper bound of the Manhattan distance a chain of RoomCount Path rooms can cover between its entry and exit cells. */
		int32 Reach(const int32 RoomCount) const
		{
			return RoomCount * 2 * MaxPathSpan;
		}

		/** Free connectors that open onto a free cell (a bridge room can stand in front of them). */
		TArray<FFreeConnector> UsableFreeConnectors() const
		{
			TArray<FFreeConnector> Usable;
			for (int32 RoomIndex = 0; RoomIndex < Rooms.Num(); ++RoomIndex)
			{
				for (const int32 Connector : FreeConnectors(RoomIndex))
				{
					const FFreeConnector Free{RoomIndex, Connector};
					if (!Occupied.Contains(OutsideCell(Free)))
					{
						Usable.Add(Free);
					}
				}
			}
			return Usable;
		}

		/** Manhattan distance from Cell to the closest cell in front of a usable free connector of another room than Excluded. */
		int32 NearestTarget(const FIntPoint& Cell, const int32 Excluded) const
		{
			int32 Best = MAX_int32;
			for (const FFreeConnector& Target : UsableFreeConnectors())
			{
				if (Target.Room != Excluded)
				{
					Best = FMath::Min(Best, Manhattan(Cell, OutsideCell(Target)));
				}
			}
			return Best;
		}

		/** Attaches Path rooms to From until one faces a free connector of another room; undoes everything on failure. */
		bool Bridge(const FFreeConnector& From, const int32 RoomsLeft)
		{
			if (Rooms.Num() >= Rules.MaxRooms || OutOfBudget() || NearestTarget(OutsideCell(From), From.Room) > Reach(RoomsLeft))
			{
				return false;
			}

			TArray<FCandidate> Candidates;
			CollectCandidates(From.Room, From.Connector, EHollowRoomRole::Path, Candidates);
			for (const FCandidate& Candidate : Candidates)
			{
				if (OutOfBudget())
				{
					return false;
				}

				FFreeConnector Target;
				const int32 Exit = ExitFacingAny(Candidate, From.Room, Target);
				if (Exit != INDEX_NONE)
				{
					const int32 NewRoom = Place(Candidate, From.Room, From.Connector, /*bCritical=*/false);
					AddLink(NewRoom, Exit, Target.Room, Target.Connector);
					if (KeepsRouteLength())
					{
						return true;
					}
					PopRoom();
					continue;
				}

				if (RoomsLeft > 1 && CanReach(Candidate, From.Room, RoomsLeft - 1))
				{
					const int32 NewRoom = Place(Candidate, From.Room, From.Connector, /*bCritical=*/false);
					for (const int32 Next : FreeConnectors(NewRoom))
					{
						if (Bridge({NewRoom, Next}, RoomsLeft - 1))
						{
							return true;
						}
					}
					PopRoom();
				}
			}
			return false;
		}

		/** World connectors a candidate would have, without placing it. */
		void PreviewConnectors(const FCandidate& Candidate, TArray<FWorldConnector>& Out) const
		{
			const FHollowRoomTemplate& Template = Templates[Candidate.Template];
			Out.Reset();
			for (const FHollowTemplateConnector& Connector : Template.Connectors)
			{
				FWorldConnector& World = Out.AddDefaulted_GetRef();
				World.Cell = Candidate.Cell + RotateCell(Connector.Cell, Template.Footprint, Candidate.Yaw90);
				World.Side = RotateSide(Connector.Side, Candidate.Yaw90);
				World.Kinds = Connector.Kinds;
			}
		}

		/**
		 * Exit of the candidate (not its entry) that would face a free, kind-compatible connector of a room other than
		 * AttachedTo; INDEX_NONE if there is none. The candidate is not placed.
		 */
		int32 ExitFacingAny(const FCandidate& Candidate, const int32 AttachedTo, FFreeConnector& OutTarget) const
		{
			TArray<FWorldConnector> Preview;
			PreviewConnectors(Candidate, Preview);
			for (int32 Index = 0; Index < Preview.Num(); ++Index)
			{
				if (Index == Candidate.Connector)
				{
					continue;
				}
				const FFreeConnector Facing = FindFacing(Preview[Index]);
				if (Facing.Room != INDEX_NONE && Facing.Room != AttachedTo)
				{
					OutTarget = Facing;
					return Index;
				}
			}
			return INDEX_NONE;
		}

		/** True if some exit of the candidate opens onto a free cell within reach of a target for RoomsLeft more rooms. */
		bool CanReach(const FCandidate& Candidate, const int32 AttachedTo, const int32 RoomsLeft) const
		{
			const FIntPoint Size = RotateSize(Templates[Candidate.Template].Footprint, Candidate.Yaw90);
			TArray<FWorldConnector> Preview;
			PreviewConnectors(Candidate, Preview);
			for (int32 Index = 0; Index < Preview.Num(); ++Index)
			{
				const FIntPoint Outside = Preview[Index].Cell + Offset(Preview[Index].Side);
				const bool bInsideCandidate = Outside.X >= Candidate.Cell.X && Outside.Y >= Candidate.Cell.Y
					&& Outside.X < Candidate.Cell.X + Size.X && Outside.Y < Candidate.Cell.Y + Size.Y;
				if (Index != Candidate.Connector && !bInsideCandidate && !Occupied.Contains(Outside)
					&& NearestTarget(Outside, INDEX_NONE) <= Reach(RoomsLeft))
				{
					return true;
				}
			}
			return false;
		}

		// ------------------------------------------------------------------ placement

		/** Every way to attach a Role template to the free connector, in a random order that gives each template the same chance. */
		void CollectCandidates(const int32 RoomIndex, const int32 ConnectorIndex, const EHollowRoomRole Role, TArray<FCandidate>& Out)
		{
			const FWorldConnector& From = Rooms[RoomIndex].Connectors[ConnectorIndex];
			const FIntPoint Target = From.Cell + Offset(From.Side);
			const EHollowRoomSide Needed = Opposite(From.Side);

			TArray<int32> TemplateOrder = ByRole[RoleIndex(Role)];
			Rng.Shuffle(TemplateOrder);
			for (const int32 TemplateIndex : TemplateOrder)
			{
				const FHollowRoomTemplate& Template = Templates[TemplateIndex];
				const int32 FirstOfTemplate = Out.Num();
				for (uint8 Yaw90 = 0; Yaw90 < SideCount; ++Yaw90)
				{
					const FIntPoint Size = RotateSize(Template.Footprint, Yaw90);
					for (int32 Index = 0; Index < Template.Connectors.Num(); ++Index)
					{
						const FHollowTemplateConnector& Connector = Template.Connectors[Index];
						if (RotateSide(Connector.Side, Yaw90) != Needed || (Connector.Kinds & From.Kinds) == 0)
						{
							continue;
						}
						const FIntPoint Origin = Target - RotateCell(Connector.Cell, Template.Footprint, Yaw90);
						if (IsFree(Origin, Size))
						{
							Out.Add({TemplateIndex, Yaw90, Index, Origin});
						}
					}
				}
				// Shuffle within the template only, so templates keep the order drawn above.
				TArray<FCandidate> Group(Out.GetData() + FirstOfTemplate, Out.Num() - FirstOfTemplate);
				Rng.Shuffle(Group);
				for (int32 Index = 0; Index < Group.Num(); ++Index)
				{
					Out[FirstOfTemplate + Index] = Group[Index];
				}
			}
		}

		bool IsFree(const FIntPoint& Cell, const FIntPoint& Size) const
		{
			for (int32 X = 0; X < Size.X; ++X)
			{
				for (int32 Y = 0; Y < Size.Y; ++Y)
				{
					if (Occupied.Contains(Cell + FIntPoint(X, Y)))   // lookup only, never iterated (CLAUDE.md sec. 5)
					{
						return false;
					}
				}
			}
			if (Rooms.IsEmpty())
			{
				return true;
			}
			const FIntPoint Min(FMath::Min(BoundsMin.X, Cell.X), FMath::Min(BoundsMin.Y, Cell.Y));
			const FIntPoint Max(FMath::Max(BoundsMax.X, Cell.X + Size.X), FMath::Max(BoundsMax.Y, Cell.Y + Size.Y));
			return Max.X - Min.X <= Rules.MaxExtentCells && Max.Y - Min.Y <= Rules.MaxExtentCells;
		}

		int32 Place(const FCandidate& Candidate, const int32 FromRoom, const int32 FromConnector, const bool bCritical)
		{
			++Placements;
			const FHollowRoomTemplate& Template = Templates[Candidate.Template];
			const int32 NewRoom = Rooms.Num();
			FRoomState& Room = Rooms.AddDefaulted_GetRef();
			Room.Template = Candidate.Template;
			Room.Cell = Candidate.Cell;
			Room.Size = RotateSize(Template.Footprint, Candidate.Yaw90);
			Room.Yaw90 = Candidate.Yaw90;
			Room.bCritical = bCritical;
			PreviewConnectors(Candidate, Room.Connectors);
			for (int32 X = 0; X < Room.Size.X; ++X)
			{
				for (int32 Y = 0; Y < Room.Size.Y; ++Y)
				{
					Occupied.Add(Room.Cell + FIntPoint(X, Y), NewRoom);
				}
			}
			RecomputeBounds();
			if (FromRoom != INDEX_NONE)
			{
				AddLink(FromRoom, FromConnector, NewRoom, Candidate.Connector);
			}
			return NewRoom;
		}

		/** Undoes the last placement and every link made since it (links are a stack too). */
		void PopRoom()
		{
			const int32 Last = Rooms.Num() - 1;
			while (!Links.IsEmpty() && (Links.Last().RoomA == Last || Links.Last().RoomB == Last))
			{
				UnlinkLast();
			}
			ensureMsgf(Algo::NoneOf(Links, [Last](const FLink& Link) { return Link.RoomA == Last || Link.RoomB == Last; }),
				TEXT("Sector generator: undone room is still linked"));
			const FRoomState& Room = Rooms.Last();
			for (int32 X = 0; X < Room.Size.X; ++X)
			{
				for (int32 Y = 0; Y < Room.Size.Y; ++Y)
				{
					Occupied.Remove(Room.Cell + FIntPoint(X, Y));
				}
			}
			Rooms.Pop(EAllowShrinking::No);
			RecomputeBounds();
		}

		void AddLink(const int32 RoomA, const int32 ConnectorA, const int32 RoomB, const int32 ConnectorB)
		{
			FLink& Link = Links.AddDefaulted_GetRef();
			Link.RoomA = RoomA;
			Link.ConnectorA = ConnectorA;
			Link.RoomB = RoomB;
			Link.ConnectorB = ConnectorB;
			Link.Kinds = Rooms[RoomA].Connectors[ConnectorA].Kinds & Rooms[RoomB].Connectors[ConnectorB].Kinds;
			Rooms[RoomA].Connectors[ConnectorA].Link = Links.Num() - 1;
			Rooms[RoomB].Connectors[ConnectorB].Link = Links.Num() - 1;
		}

		void UnlinkLast()
		{
			const FLink Link = Links.Pop(EAllowShrinking::No);
			Rooms[Link.RoomA].Connectors[Link.ConnectorA].Link = INDEX_NONE;
			Rooms[Link.RoomB].Connectors[Link.ConnectorB].Link = INDEX_NONE;
		}

		void RecomputeBounds()
		{
			if (Rooms.IsEmpty())
			{
				return;
			}
			BoundsMin = Rooms[0].Cell;
			BoundsMax = Rooms[0].Cell + Rooms[0].Size;
			for (const FRoomState& Room : Rooms)
			{
				BoundsMin = FIntPoint(FMath::Min(BoundsMin.X, Room.Cell.X), FMath::Min(BoundsMin.Y, Room.Cell.Y));
				BoundsMax = FIntPoint(FMath::Max(BoundsMax.X, Room.Cell.X + Room.Size.X), FMath::Max(BoundsMax.Y, Room.Cell.Y + Room.Size.Y));
			}
		}

		// ------------------------------------------------------------------ queries

		TArray<int32> FreeConnectors(const int32 RoomIndex) const
		{
			TArray<int32> Free;
			for (int32 Index = 0; Index < Rooms[RoomIndex].Connectors.Num(); ++Index)
			{
				if (Rooms[RoomIndex].Connectors[Index].Link == INDEX_NONE)
				{
					Free.Add(Index);
				}
			}
			return Free;
		}

		const FWorldConnector& ConnectorOf(const FFreeConnector& Free) const
		{
			return Rooms[Free.Room].Connectors[Free.Connector];
		}

		FIntPoint OutsideCell(const FFreeConnector& Free) const
		{
			return ConnectorOf(Free).Cell + Offset(ConnectorOf(Free).Side);
		}

		/** The free, kind-compatible connector of another room that faces this one across its side, if any. */
		FFreeConnector FindFacing(const FWorldConnector& From) const
		{
			const FIntPoint Neighbour = From.Cell + Offset(From.Side);
			const int32* Room = Occupied.Find(Neighbour);
			if (!Room)
			{
				return {};
			}
			const TArray<FWorldConnector>& Others = Rooms[*Room].Connectors;
			for (int32 Index = 0; Index < Others.Num(); ++Index)
			{
				if (Others[Index].Link == INDEX_NONE && Others[Index].Cell == Neighbour && Others[Index].Side == Opposite(From.Side)
					&& (Others[Index].Kinds & From.Kinds) != 0)
				{
					return {*Room, Index};
				}
			}
			return {};
		}

		bool AreLinked(const int32 A, const int32 B) const
		{
			return Algo::AnyOf(Links, [A, B](const FLink& Link)
			{
				return (Link.RoomA == A && Link.RoomB == B) || (Link.RoomA == B && Link.RoomB == A);
			});
		}

		TArray<FIntPoint> LinkPairs() const
		{
			TArray<FIntPoint> Result;
			for (const FLink& Link : Links)
			{
				Result.Add(FIntPoint(Link.RoomA, Link.RoomB));
			}
			return Result;
		}

		int32 CountLoops() const
		{
			return HollowSectorGraph::CountLoops(Rooms.Num(), LinkPairs());
		}

		/** Loops must not shortcut the critical path below CriticalPathMin rooms. */
		bool KeepsRouteLength() const
		{
			return HollowSectorGraph::ShortestPath(Rooms.Num(), LinkPairs(), SpawnRoom, ExtractionRoom) >= Rules.CriticalPathMin + 1;
		}

		const TArray<FHollowRoomTemplate>& Templates;
		const FHollowSectorGenRules& Rules;
		FHollowRandom Rng;

		TArray<int32> ByRole[RoleCount];
		int32 MaxPathSpan = 1;

		TArray<FRoomState> Rooms;
		TArray<FLink> Links;
		TMap<FIntPoint, int32> Occupied;
		FIntPoint BoundsMin = FIntPoint::ZeroValue;
		FIntPoint BoundsMax = FIntPoint::ZeroValue;
		int32 Placements = 0;

		int32 SpawnRoom = INDEX_NONE;
		int32 ExtractionRoom = INDEX_NONE;
		TArray<int32> CriticalInterior;
	};

	/** Final acceptance of an attempt; independent of how it was built. */
	bool Accept(const FHollowSectorStats& Stats, const FHollowSectorGenRules& Rules)
	{
		return Stats.bOverlapFree
			&& Stats.Components == 1
			&& Stats.SpawnRooms == 1
			&& Stats.ExtractionRooms == 1
			&& Stats.CoreVaults == Rules.CoreVaultCount
			&& Stats.SpawnToExtraction >= Rules.CriticalPathMin + 1
			&& Stats.Loops >= Rules.MinLoops
			&& Stats.DeadEndDepth <= Rules.MaxBranchDepth
			&& Stats.Rooms <= Rules.MaxRooms
			&& Stats.Bulkheads >= Rules.MinBulkheadConnections
			&& Stats.Extent.X <= Rules.MaxExtentCells
			&& Stats.Extent.Y <= Rules.MaxExtentCells;
	}

	void AppendInt(TArray<uint8>& Bytes, const int32 Value)
	{
		const uint32 Bits = static_cast<uint32>(Value);
		for (int32 Shift = 0; Shift < 32; Shift += 8)   // little-endian on every platform
		{
			Bytes.Add(static_cast<uint8>((Bits >> Shift) & 0xFFu));
		}
	}

	TCHAR RoomLetter(const int32 Index)
	{
		constexpr int32 Letters = 26;
		return Index < Letters ? static_cast<TCHAR>(TEXT('A') + Index) : TEXT('?');
	}
}

TArray<FString> FHollowSectorGenerator::ValidateRules(const FHollowSectorGenRules& R)
{
	TArray<FString> Errors;
	auto Require = [&Errors](const bool bOk, const TCHAR* What) { if (!bOk) { Errors.Add(What); } };
	Require(R.SpawnRoomType.IsValid() && R.ExtractionRoomType.IsValid(), TEXT("Spawn and Extraction room types must be set."));
	Require(R.CoreVaultCount == 0 || R.CoreVaultRoomType.IsValid(), TEXT("CoreVaultRoomType must be set when CoreVaultCount > 0."));
	Require(!R.PathRoomTypes.IsEmpty(), TEXT("PathRoomTypes must not be empty."));
	Require(R.CriticalPathMin >= 1 && R.CriticalPathMin <= R.CriticalPathMax, TEXT("Need 1 <= CriticalPathMin <= CriticalPathMax."));
	Require(R.CoreVaultCount >= 0, TEXT("CoreVaultCount must not be negative."));
	Require(R.OptionalBranchesMin >= 0 && R.OptionalBranchesMin <= R.OptionalBranchesMax, TEXT("Need 0 <= OptionalBranchesMin <= OptionalBranchesMax."));
	Require(R.OptionalBranchesMin == 0 || !R.BranchRoomTypes.IsEmpty(), TEXT("BranchRoomTypes must be set when OptionalBranchesMin > 0."));
	Require(R.MaxBranchDepth >= 1, TEXT("MaxBranchDepth must be at least 1."));
	Require(R.MinLoops >= 0, TEXT("MinLoops must not be negative."));
	Require(R.MinLoops == 0 || R.MaxBridgeRooms >= 1, TEXT("MaxBridgeRooms must be at least 1 when MinLoops > 0."));
	Require(R.MinBulkheadConnections >= 0, TEXT("MinBulkheadConnections must not be negative."));
	Require(R.MaxRooms >= R.CriticalPathMax + 2 + R.CoreVaultCount, TEXT("MaxRooms must fit Spawn, Extraction, the longest critical path and the core vaults."));
	Require(R.MaxExtentCells >= 1, TEXT("MaxExtentCells must be at least 1."));
	// The sector manifest stores cells and room indices as uint8 (ADR-0003: < 2 KB on the wire).
	Require(R.MaxExtentCells <= TNumericLimits<uint8>::Max() && R.MaxRooms <= TNumericLimits<uint8>::Max(),
		TEXT("MaxExtentCells and MaxRooms must fit the manifest (255)."));
	Require(R.MaxAttempts >= 1, TEXT("MaxAttempts must be at least 1."));
	Require(R.MaxPlacementsPerAttempt >= 1, TEXT("MaxPlacementsPerAttempt must be at least 1."));
	return Errors;
}

TArray<FString> FHollowSectorGenerator::ValidateInput(const FHollowSectorGenInput& Input)
{
	TArray<FString> Errors = ValidateRules(Input.Rules);
	int32 RoleCounts[HollowSectorGenPrivate::RoleCount] = {};
	TArray<FString> Ids;
	for (const FHollowRoomTemplate& Template : Input.Templates)
	{
		if (Template.Id.IsEmpty() || Ids.Contains(Template.Id))
		{
			Errors.Add(FString::Printf(TEXT("Template ids must be unique and non-empty ('%s')."), *Template.Id));
		}
		Ids.Add(Template.Id);
		++RoleCounts[HollowSectorGenPrivate::RoleIndex(Template.Role)];
		if (Template.Footprint.X < 1 || Template.Footprint.Y < 1 || Template.Connectors.IsEmpty())
		{
			Errors.Add(FString::Printf(TEXT("%s: needs a footprint and at least one connector."), *Template.Id));
		}
		for (const FHollowTemplateConnector& Connector : Template.Connectors)
		{
			const FIntPoint& C = Connector.Cell;
			const bool bInside = C.X >= 0 && C.Y >= 0 && C.X < Template.Footprint.X && C.Y < Template.Footprint.Y;
			const FIntPoint Outside = C + HollowGridMath::Offset(Connector.Side);
			const bool bFacesOut = Outside.X < 0 || Outside.Y < 0 || Outside.X >= Template.Footprint.X || Outside.Y >= Template.Footprint.Y;
			if (!bInside || !bFacesOut || Connector.Kinds == 0)
			{
				Errors.Add(FString::Printf(TEXT("%s: connector at (%d,%d) must be on the outer boundary and allow a kind."), *Template.Id, C.X, C.Y));
			}
		}
	}
	auto Count = [&RoleCounts](const EHollowRoomRole Role) { return RoleCounts[HollowSectorGenPrivate::RoleIndex(Role)]; };
	if (Count(EHollowRoomRole::Spawn) == 0 || Count(EHollowRoomRole::Extraction) == 0 || Count(EHollowRoomRole::Path) == 0)
	{
		Errors.Add(TEXT("The pool needs Spawn, Extraction and Path templates."));
	}
	if (Input.Rules.CoreVaultCount > 0 && Count(EHollowRoomRole::CoreVault) == 0)
	{
		Errors.Add(TEXT("The pool needs a CoreVault template."));
	}
	if (Input.Rules.OptionalBranchesMin > 0 && Count(EHollowRoomRole::Branch) == 0)
	{
		Errors.Add(TEXT("The pool needs a Branch template."));
	}
	return Errors;
}

EHollowGenResult FHollowSectorGenerator::Generate(const FHollowSectorGenInput& Input, FHollowSectorLayout& OutLayout, FHollowSectorGenReport* OutReport)
{
	FHollowSectorGenReport LocalReport;
	FHollowSectorGenReport& Report = OutReport ? *OutReport : LocalReport;
	Report = FHollowSectorGenReport();

	Report.InputErrors = ValidateInput(Input);
	if (!Report.InputErrors.IsEmpty())
	{
		for (const FString& Error : Report.InputErrors)
		{
			UE_LOG(LogHollowPCG, Error, TEXT("Sector generator input: %s"), *Error);
		}
		return EHollowGenResult::InvalidInput;
	}

	// Pool order is part of determinism: never trust the caller's (asset registry) order.
	TArray<FHollowRoomTemplate> Templates = Input.Templates;
	Templates.Sort([](const FHollowRoomTemplate& A, const FHollowRoomTemplate& B) { return A.Id.Compare(B.Id, ESearchCase::CaseSensitive) < 0; });

	for (int32 Attempt = 0; Attempt < Input.Rules.MaxAttempts; ++Attempt)
	{
		HollowSectorGenPrivate::FAttempt AttemptRun(Templates, Input.Rules, HollowRandom::DeriveSeed(Input.Seed, Attempt));
		EHollowGenFailure Failure = AttemptRun.Run();
		if (Failure == EHollowGenFailure::None)
		{
			FHollowSectorLayout Layout;
			AttemptRun.Export(Layout);
			if (HollowSectorGenPrivate::Accept(HollowSectorGraph::Analyze(Layout), Input.Rules))
			{
				Layout.Seed = Input.Seed;
				Layout.Attempts = Attempt + 1;
				Layout.LayoutHash = ComputeLayoutHash(Layout);
				OutLayout = MoveTemp(Layout);
				return EHollowGenResult::Success;
			}
			Failure = EHollowGenFailure::Validation;
		}
		Report.AttemptFailures.Add(Failure);
		UE_LOG(LogHollowPCG, Verbose, TEXT("Sector seed %d attempt %d failed: %s"), Input.Seed, Attempt, *HollowEnumToString(Failure));
	}
	UE_LOG(LogHollowPCG, Warning, TEXT("Sector seed %d: no valid layout in %d attempts."), Input.Seed, Input.Rules.MaxAttempts);
	return EHollowGenResult::ExhaustedAttempts;
}

uint32 FHollowSectorGenerator::ComputeLayoutHash(const FHollowSectorLayout& Layout)
{
	using HollowSectorGenPrivate::AppendInt;
	TArray<uint8> Bytes;
	for (const FHollowPlacedRoom& Room : Layout.Rooms)
	{
		const FTCHARToUTF8 Id(Layout.TemplateIds.IsValidIndex(Room.Template) ? *Layout.TemplateIds[Room.Template] : TEXT(""));
		Bytes.Append(reinterpret_cast<const uint8*>(Id.Get()), Id.Length());
		Bytes.Add(0);
		AppendInt(Bytes, Room.Cell.X);
		AppendInt(Bytes, Room.Cell.Y);
		AppendInt(Bytes, Room.Size.X);
		AppendInt(Bytes, Room.Size.Y);
		AppendInt(Bytes, Room.Yaw90);
		AppendInt(Bytes, static_cast<int32>(Room.Role));
	}
	for (const FHollowPlacedConnection& Connection : Layout.Connections)
	{
		AppendInt(Bytes, Connection.RoomA);
		AppendInt(Bytes, Connection.ConnectorA);
		AppendInt(Bytes, Connection.RoomB);
		AppendInt(Bytes, Connection.ConnectorB);
		AppendInt(Bytes, static_cast<int32>(Connection.Kind));
	}
	return FCrc::MemCrc32(Bytes.GetData(), Bytes.Num());
}

TArray<FString> FHollowSectorGenerator::RenderAscii(const FHollowSectorLayout& Layout)
{
	using namespace HollowSectorGenPrivate;

	FIntPoint Extent = FIntPoint::ZeroValue;
	for (const FHollowPlacedRoom& Room : Layout.Rooms)
	{
		Extent = FIntPoint(FMath::Max(Extent.X, Room.Cell.X + Room.Size.X), FMath::Max(Extent.Y, Room.Cell.Y + Room.Size.Y));
	}

	// Two characters per cell: cell centres on odd positions, walls and openings between them. North (+X) is up.
	TArray<FString> Grid;
	for (int32 Row = 0; Row < 2 * Extent.X + 1; ++Row)
	{
		Grid.Add(FString::ChrN(2 * Extent.Y + 1, TEXT(' ')));
	}
	auto Put = [&Grid, &Extent](const int32 DoubledX, const int32 DoubledY, const TCHAR Char)
	{
		const int32 Row = 2 * Extent.X - DoubledX;
		if (Grid.IsValidIndex(Row) && DoubledY >= 0 && DoubledY < Grid[Row].Len())
		{
			Grid[Row][DoubledY] = Char;
		}
	};

	for (int32 Index = 0; Index < Layout.Rooms.Num(); ++Index)
	{
		const FHollowPlacedRoom& Room = Layout.Rooms[Index];
		for (int32 X = 2 * Room.Cell.X + 1; X < 2 * (Room.Cell.X + Room.Size.X); ++X)
		{
			for (int32 Y = 2 * Room.Cell.Y + 1; Y < 2 * (Room.Cell.Y + Room.Size.Y); ++Y)
			{
				Put(X, Y, RoomLetter(Index));
			}
		}
	}
	for (const FHollowPlacedConnection& Connection : Layout.Connections)
	{
		const FIntPoint Step = HollowGridMath::Offset(Connection.SideA);
		const TCHAR Mark = Connection.Kind == EHollowConnectorKind::Bulkhead ? TEXT('#') : Connection.Kind == EHollowConnectorKind::Open ? TEXT(':') : TEXT('+');
		Put(2 * Connection.CellA.X + 1 + Step.X, 2 * Connection.CellA.Y + 1 + Step.Y, Mark);
	}

	TArray<FString> Lines;
	for (const FString& Row : Grid)
	{
		Lines.Add(TEXT("  ") + Row);
	}
	Lines.Add(TEXT("  legend: + door, # bulkhead, : open; north (+X) is up"));
	for (int32 Index = 0; Index < Layout.Rooms.Num(); ++Index)
	{
		const FHollowPlacedRoom& Room = Layout.Rooms[Index];
		Lines.Add(FString::Printf(TEXT("  %c %-10s %s at (%d,%d) yaw %d%s"), RoomLetter(Index), *HollowEnumToString(Room.Role),
			Layout.TemplateIds.IsValidIndex(Room.Template) ? *Layout.TemplateIds[Room.Template] : TEXT("?"), Room.Cell.X, Room.Cell.Y,
			Room.Yaw90 * 90, Room.bCriticalPath ? TEXT(" [critical]") : TEXT("")));
	}
	const FHollowSectorStats Stats = HollowSectorGraph::Analyze(Layout);
	Lines.Add(FString::Printf(TEXT("  seed %d: %d rooms, %d links, %d loops, route %d links, dead end %d, extent %dx%d, %d bulkheads, %d sealed, attempts %d, hash %08X"),
		Layout.Seed, Stats.Rooms, Stats.Links, Stats.Loops, Stats.SpawnToExtraction, Stats.DeadEndDepth, Stats.Extent.X, Stats.Extent.Y,
		Stats.Bulkheads, Layout.Sealed.Num(), Layout.Attempts, Layout.LayoutHash));
	return Lines;
}
