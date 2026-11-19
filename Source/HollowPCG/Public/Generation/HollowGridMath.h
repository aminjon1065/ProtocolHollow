#pragma once

#include "Data/HollowRoomTypes.h"
#include "Math/IntPoint.h"
#include "Math/UnrealMathUtility.h"

/**
 * Integer grid math of the sector generator (Docs/QA/LevelMetrics.md sec. 2). A room rotated by Yaw90 is turned by
 * Yaw90 * 90 degrees of yaw (+X towards +Y) and then shifted so that its rotated footprint starts at its cell.
 */
namespace HollowGridMath
{
	constexpr int32 SideCount = 4;

	inline EHollowRoomSide RotateSide(const EHollowRoomSide Side, const uint8 Yaw90)
	{
		return static_cast<EHollowRoomSide>((static_cast<int32>(Side) + Yaw90) % SideCount);
	}

	inline EHollowRoomSide Opposite(const EHollowRoomSide Side)
	{
		return RotateSide(Side, SideCount / 2);
	}

	/** Step to the neighbouring cell across a side. */
	inline FIntPoint Offset(const EHollowRoomSide Side)
	{
		switch (Side)
		{
		case EHollowRoomSide::North: return FIntPoint(1, 0);
		case EHollowRoomSide::East:  return FIntPoint(0, 1);
		case EHollowRoomSide::South: return FIntPoint(-1, 0);
		case EHollowRoomSide::West:  return FIntPoint(0, -1);
		}
		return FIntPoint::ZeroValue;
	}

	inline FIntPoint RotateSize(const FIntPoint& Footprint, const uint8 Yaw90)
	{
		return (Yaw90 % 2) != 0 ? FIntPoint(Footprint.Y, Footprint.X) : Footprint;
	}

	/** Cell of a footprint after rotation, relative to the rotated footprint's min corner. */
	inline FIntPoint RotateCell(FIntPoint Cell, FIntPoint Footprint, const uint8 Yaw90)
	{
		for (int32 Step = 0; Step < Yaw90 % SideCount; ++Step)
		{
			Cell = FIntPoint(Footprint.Y - 1 - Cell.Y, Cell.X);   // +90 deg yaw: (x, y) -> (-y, x), then shift into the footprint
			Footprint = FIntPoint(Footprint.Y, Footprint.X);
		}
		return Cell;
	}

	/**
	 * Where the room's own origin lands, in cells from its rotated footprint's min corner: the Level Instance of a room
	 * placed at Cell with Yaw90 goes to (Cell + PivotOffset) * CellSize with yaw Yaw90 * 90 (assembly, S2-02).
	 */
	inline FIntPoint PivotOffset(const FIntPoint& Footprint, const uint8 Yaw90)
	{
		switch (Yaw90 % SideCount)
		{
		case 1:  return FIntPoint(Footprint.Y, 0);
		case 2:  return FIntPoint(Footprint.X, Footprint.Y);
		case 3:  return FIntPoint(0, Footprint.X);
		default: return FIntPoint::ZeroValue;
		}
	}

	inline int32 Manhattan(const FIntPoint& A, const FIntPoint& B)
	{
		return FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y);
	}
}
