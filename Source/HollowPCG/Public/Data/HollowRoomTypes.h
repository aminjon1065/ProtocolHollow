#pragma once

#include "GameplayTagContainer.h"
#include "Math/IntVector.h"
#include "Math/Transform.h"

#include "HollowRoomTypes.generated.h"

/**
 * Side of a grid cell. Room-local axes: North = +X, East = +Y, South = -X, West = -Y.
 * Room-local origin: the -X/-Y corner of cell (0,0,0) on the floor top (Docs/QA/LevelMetrics.md).
 */
UENUM(BlueprintType)
enum class EHollowRoomSide : uint8
{
	North,
	East,
	South,
	West
};

/** What may fill a connector when two rooms join (the generator picks one allowed by both sides). */
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EHollowConnectorKind : uint8
{
	None = 0 UMETA(Hidden),
	Door = 1 << 0,
	Bulkhead = 1 << 1,
	Open = 1 << 2
};
ENUM_CLASS_FLAGS(EHollowConnectorKind);

/** A standard opening (LevelMetrics DoorWidth x DoorHeight) centered on one outer edge of a footprint cell. */
USTRUCT(BlueprintType)
struct FHollowRoomConnector
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connector")
	FIntVector Cell = FIntVector::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connector")
	EHollowRoomSide Side = EHollowRoomSide::North;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connector", meta = (Bitmask, BitmaskEnum = "/Script/HollowPCG.EHollowConnectorKind"))
	uint8 AllowedKinds = static_cast<uint8>(EHollowConnectorKind::Door | EHollowConnectorKind::Bulkhead | EHollowConnectorKind::Open);
};

/** Room-local anchor where the server spawns a gameplay actor (loot, core, enemy, extraction...). */
USTRUCT(BlueprintType)
struct FHollowRoomSocket
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Socket", meta = (Categories = "Hollow.Socket"))
	FGameplayTag SocketType;

	/** Room-local; location on the floor top (Z = 0) unless the socket type says otherwise. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Socket")
	FTransform Transform;
};
