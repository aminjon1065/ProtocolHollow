#pragma once

#include "CoreMinimal.h"

#include "HollowBreachTypes.generated.h"

class UPackageMap;

/** Bulkhead states (Docs/Architecture/03 sec. 3.4). Breached is terminal: the passage is open for good. */
UENUM(BlueprintType)
enum class EHollowBreachState : uint8
{
	Intact,
	/** Damaged (cracks), or some zones broken while the passage is still shut. */
	Weakened,
	Breached
};

/** How one machine shows a zone that broke (decided locally, never replicated). */
UENUM(BlueprintType)
enum class EHollowBreachPresentation : uint8
{
	/** Straight to the broken state: no simulation (late join, far away, unseen, over budget). */
	Snap,
	/** Live cosmetic Chaos fracture of the zone. */
	Full
};

/**
 * Everything a client needs to show a bulkhead, 9 bytes on the wire (ADR-0002: state, not simulation). The zone
 * mask decides gameplay collision on every machine; the impact direction and the time only shape the cosmetics.
 */
USTRUCT(BlueprintType)
struct HOLLOWPHYSICS_API FHollowBreachNetState
{
	GENERATED_BODY()

	/** State and zone mask share one byte on the wire. */
	static constexpr int32 MaxZones = 6;

	UPROPERTY(BlueprintReadOnly, Category = "Breach")
	EHollowBreachState State = EHollowBreachState::Intact;

	/** Bit per broken zone. */
	UPROPERTY(BlueprintReadOnly, Category = "Breach")
	uint8 ZoneMask = 0;

	/** Share of the bulkhead's total health lost [0..1], quantized: cracks before anything breaks. */
	UPROPERTY(BlueprintReadOnly, Category = "Breach")
	uint8 DamageStageQ = 0;

	/** Direction of the blow that broke the last zone: yaw and pitch, a byte each (debris flies along it). */
	UPROPERTY(BlueprintReadOnly, Category = "Breach")
	uint8 ImpactYawQ = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Breach")
	uint8 ImpactPitchQ = 0;

	/** Server time the last zone broke: its age decides between a live fracture and a snap. */
	UPROPERTY(BlueprintReadOnly, Category = "Breach")
	float BreachServerTime = 0.f;

	/** Incremented whenever a zone breaks, so every machine presents each break exactly once. */
	UPROPERTY(BlueprintReadOnly, Category = "Breach")
	uint8 TransitionSerial = 0;

	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);

	bool IsZoneBroken(const int32 Zone) const { return Zone >= 0 && Zone < MaxZones && (ZoneMask & (1u << Zone)) != 0; }

	FVector GetImpactDirection() const;
	void SetImpactDirection(const FVector& Direction);

	bool operator==(const FHollowBreachNetState&) const = default;
};

template <>
struct TStructOpsTypeTraits<FHollowBreachNetState> : public TStructOpsTypeTraitsBase2<FHollowBreachNetState>
{
	enum
	{
		WithNetSerializer = true,
		WithIdenticalViaEquality = true
	};
};
