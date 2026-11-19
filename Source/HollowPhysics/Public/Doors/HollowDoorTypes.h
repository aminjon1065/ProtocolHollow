#pragma once

#include "CoreMinimal.h"

#include "HollowDoorTypes.generated.h"

class UPackageMap;

/** Door states (Docs/Architecture/03 sec. 3.3). Breached is terminal (structural damage, S3). */
UENUM(BlueprintType)
enum class EHollowDoorState : uint8
{
	Open,
	Opening,
	Closing,
	Closed,
	Welded,
	Breached
};

UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EHollowDoorFlags : uint8
{
	None = 0 UMETA(Hidden),
	Powered = 1 << 0,
	Locked = 1 << 1,
	Barricaded = 1 << 2
};
ENUM_CLASS_FLAGS(EHollowDoorFlags);

/**
 * Everything a client needs to show a door, replicated as one 8-byte unit (ADR-0002: state, not simulation).
 * State and flags share a byte, so a state can never arrive without its transition time.
 */
USTRUCT(BlueprintType)
struct HOLLOWPHYSICS_API FHollowDoorNetState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Door")
	EHollowDoorState State = EHollowDoorState::Closed;

	/** EHollowDoorFlags. */
	UPROPERTY(BlueprintReadOnly, Category = "Door")
	uint8 Flags = 0;

	/** Weld integrity [0..1] quantized to a byte. */
	UPROPERTY(BlueprintReadOnly, Category = "Door")
	uint8 WeldQ = 0;

	/** Structure [0..1] quantized; full until structural damage arrives (S3). */
	UPROPERTY(BlueprintReadOnly, Category = "Door")
	uint8 StructureQ = TNumericLimits<uint8>::Max();

	/** Incremented on every state change, so one-shot effects play exactly once even if states were skipped. */
	UPROPERTY(BlueprintReadOnly, Category = "Door")
	uint8 TransitionSerial = 0;

	/** Server time the current state began (leaf motion and effects run from it on every machine). */
	UPROPERTY(BlueprintReadOnly, Category = "Door")
	float StateServerTime = 0.f;

	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);

	bool HasFlag(const EHollowDoorFlags Flag) const { return (Flags & static_cast<uint8>(Flag)) != 0; }

	bool operator==(const FHollowDoorNetState&) const = default;
};

template <>
struct TStructOpsTypeTraits<FHollowDoorNetState> : public TStructOpsTypeTraitsBase2<FHollowDoorNetState>
{
	enum
	{
		WithNetSerializer = true,
		WithIdenticalViaEquality = true
	};
};
