#pragma once

#include "Breach/HollowBreachTypes.h"
#include "Containers/Array.h"

class UHollowBreachableDefinition;

/** What a structural blow did to a bulkhead. */
enum class EHollowBreachOutcome : uint8
{
	/** Breached already, nothing left to hit, or no damage: nothing changed. */
	Rejected,
	/** Health and maybe DamageStageQ changed; no zone broke. */
	Damaged,
	/** At least one zone broke: TransitionSerial and BreachServerTime were updated (the state may be Breached now). */
	ZoneBroken
};

/** Authoritative bulkhead state on the server: the replicated part plus each zone's exact health. */
struct FHollowBreachServerState
{
	FHollowBreachNetState Net;
	TArray<float, TInlineAllocator<FHollowBreachNetState::MaxZones>> Health;
};

/** One blow in actor space. */
struct FHollowBreachHit
{
	FVector LocalPoint = FVector::ZeroVector;
	float Amount = 0.f;
	/** World direction the debris should fly. */
	FVector Direction = FVector::ForwardVector;
	float ServerTime = 0.f;
};

/** Pure bulkhead rules (Docs/Architecture/03 sec. 3.4, CLAUDE.md sec. 7.2). */
namespace HollowBreachRules
{
	/** Full health, nothing broken. */
	HOLLOWPHYSICS_API FHollowBreachServerState MakeInitial(const UHollowBreachableDefinition& Definition);

	/** The intact zone whose centre is nearest to LocalPoint (lowest index on a tie); INDEX_NONE if all are broken. */
	HOLLOWPHYSICS_API int32 PickZone(const FHollowBreachServerState& State, const FVector& LocalPoint, const UHollowBreachableDefinition& Definition);

	/** Applies a blow to one zone; damage beyond what the zone has left is lost (one blow breaks one zone at most). */
	HOLLOWPHYSICS_API EHollowBreachOutcome ApplyHit(FHollowBreachServerState& InOut, const FHollowBreachHit& Hit, const UHollowBreachableDefinition& Definition);

	HOLLOWPHYSICS_API bool IsPassageOpen(uint8 ZoneMask, const UHollowBreachableDefinition& Definition);

	/** Share of total health lost, quantized to a byte. */
	HOLLOWPHYSICS_API uint8 DamageStage(const FHollowBreachServerState& State, const UHollowBreachableDefinition& Definition);

	/** Zones broken in New that Shown has not presented yet. */
	inline uint8 NewlyBroken(const uint8 ShownMask, const FHollowBreachNetState& New) { return static_cast<uint8>(New.ZoneMask & ~ShownMask); }
}
