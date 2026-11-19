#pragma once

#include "Doors/HollowDoorTypes.h"

#include "HollowDoorRules.generated.h"

class UHollowDoorDefinition;

UENUM()
enum class EHollowDoorEvent : uint8
{
	Interact,
	Weld,
	Cut,
	OpenFinished,
	CloseFinished
};

/** What an event did to the door. */
UENUM()
enum class EHollowDoorOutcome : uint8
{
	/** Not allowed in this state: nothing changed. */
	Rejected,
	/** Allowed but no visible change (e.g. welding a fully welded door). */
	Unchanged,
	WeldChanged,
	/** State changed: TransitionSerial and StateServerTime were updated. */
	StateChanged
};

struct FHollowDoorEvent
{
	EHollowDoorEvent Type = EHollowDoorEvent::Interact;
	/** Tool work units (Weld, Cut). */
	float Amount = 0.f;
	/** Interact on an open door: closing needs a free doorway. */
	bool bDoorwayClear = true;
	float ServerTime = 0.f;
};

/** Authoritative door state on the server: the replicated part plus the exact weld behind WeldQ. */
struct FHollowDoorServerState
{
	FHollowDoorNetState Net;
	float Weld = 0.f;
};

/** Pure door rules (Docs/Architecture/03 sec. 3.3 transition table, CLAUDE.md sec. 7.2). */
namespace HollowDoorRules
{
	HOLLOWPHYSICS_API float ApplyWeld(float Weld, float WorkUnits, const UHollowDoorDefinition& Definition);
	HOLLOWPHYSICS_API float ApplyCut(float Weld, float WorkUnits, const UHollowDoorDefinition& Definition);

	HOLLOWPHYSICS_API uint8 Quantize(float Unit);
	HOLLOWPHYSICS_API float Dequantize(uint8 Quantized);

	/** Throttle of weld updates while a tool runs. */
	HOLLOWPHYSICS_API bool ShouldSendWeld(uint8 SentQ, uint8 NewQ, double SecondsSinceSend, float Interval);

	/** The transition table: applies Event to InOut (only on the server) and says what happened. */
	HOLLOWPHYSICS_API EHollowDoorOutcome Apply(FHollowDoorServerState& InOut, const FHollowDoorEvent& Event, const UHollowDoorDefinition& Definition);

	/** Apply on a copy: would the event be accepted? Used for tool stickiness and interaction prompts. */
	HOLLOWPHYSICS_API bool Accepts(const FHollowDoorServerState& State, const FHollowDoorEvent& Event, const UHollowDoorDefinition& Definition);

	/** Leaf opening 0 (closed) .. 1 (open) at ServerNow, the same on every machine. */
	HOLLOWPHYSICS_API float LeafAlpha(const FHollowDoorNetState& State, double ServerNow, const UHollowDoorDefinition& Definition);

	/** Whether the leaf blocks pawns at that opening. */
	HOLLOWPHYSICS_API bool IsLeafBlocking(EHollowDoorState State, float Alpha, const UHollowDoorDefinition& Definition);

	/** A replicated change is a fresh transition worth playing (vs. late join / regained relevancy: snap). */
	HOLLOWPHYSICS_API bool IsFreshTransition(const FHollowDoorNetState& Old, const FHollowDoorNetState& New, double ServerNow, const UHollowDoorDefinition& Definition);
}
