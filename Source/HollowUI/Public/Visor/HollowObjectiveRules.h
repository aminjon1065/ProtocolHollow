#pragma once

#include "CoreMinimal.h"
#include "Misc/EnumRange.h"
#include "Raid/HollowRaidStateTypes.h"

#include "HollowObjectiveRules.generated.h"

/** What the player should do now (UX-01, ADR-0042), one step of the hub or the raid. */
UENUM()
enum class EHollowObjectiveStep : uint8
{
	/** Neither hub nor raid (front end, a test world without either): the tracker is hidden. */
	None,
	/** Hub: look at the workbench or the locker first. */
	HubPrepare,
	/** Hub, host: launch the raid at the contracts console. */
	HubLaunch,
	/** Hub, guest: the host launches the raid. */
	HubWaitHost,
	/** The sector is built and the squad comes down. */
	Deploying,
	/** No core lifted yet: it rests in the vault. */
	FindCore,
	/** A lifted core the altar accepts, the altar free: purify it or carry it out as it is. */
	PurifyOrExtract,
	/** A core lies on the altar: the ritual runs. */
	GuardRitual,
	/** A core is out of the vault: to the extraction zone. */
	DeliverCore,
	/** The shuttle counts down. */
	HoldZone,
	/** This player is dead; the squad can still extract. */
	Down,
	/** Results are out; back to the hub soon. */
	RaidOver
};

/** A place the visor marks with its distance (UX-01). Enemies are never marked (Docs/Art bible 4E). */
UENUM()
enum class EHollowWaypointKind : uint8
{
	Contracts,
	Workbench,
	Locker,
	Extraction,
	Vault,
	Altar
};
ENUM_RANGE_BY_FIRST_AND_LAST(EHollowWaypointKind, EHollowWaypointKind::Contracts, EHollowWaypointKind::Altar);

/** Everything the objective depends on, as this machine knows it (UHollowObjectiveModel gathers it from events). */
struct HOLLOWUI_API FHollowObjectiveFacts
{
	/** A raid with an id is under way (its state replicated). */
	bool bInRaid = false;
	/** The hub: no raid, the shuttle's terminals are here. */
	bool bInHub = false;
	/** This machine decides what the host decides (listen server or solo). */
	bool bHost = false;
	/** Hub: the player opened the workbench or the locker this visit. */
	bool bPrepared = false;

	EHollowRaidPhase Phase = EHollowRaidPhase::Assembling;
	/** Seconds until the current timed phase ends (the shuttle's departure in Extraction). */
	int32 SecondsLeft = 0;
	/** The player has a living body (spectating is not alive). */
	bool bAlive = true;
	/** Some core was lifted this raid. */
	bool bCoreLifted = false;
	/** A lifted core counts as the item an altar accepts and is not on the altar. */
	bool bPurifiableCore = false;
	/** An altar waits for a core. */
	bool bAltarIdle = false;
	/** A core lies on an altar. */
	bool bRitual = false;
	/** What a plain core and a purified one bring this player home (FHollowExtractionResolver::ItemEarnings). */
	int32 CoreCredits = 0;
	int32 PurifiedCredits = 0;

	bool operator==(const FHollowObjectiveFacts&) const = default;
};

/** The step, its line on the visor and the places to mark. */
struct HOLLOWUI_API FHollowObjective
{
	EHollowObjectiveStep Step = EHollowObjectiveStep::None;
	FText Text;
	/** In the order the visor lists them. */
	TArray<EHollowWaypointKind> Waypoints;

	bool Shows(const EHollowWaypointKind Kind) const { return Waypoints.Contains(Kind); }
	bool SameAs(const FHollowObjective& Other) const { return Step == Other.Step && Waypoints == Other.Waypoints && Text.EqualTo(Other.Text); }
};

/** UX-01 (ADR-0042): the objective is a pure function of the facts. */
namespace HollowObjectiveRules
{
	HOLLOWUI_API EHollowObjectiveStep PickStep(const FHollowObjectiveFacts& Facts);
	HOLLOWUI_API TArray<EHollowWaypointKind> WaypointsFor(EHollowObjectiveStep Step);
	HOLLOWUI_API FText StepText(EHollowObjectiveStep Step, const FHollowObjectiveFacts& Facts);
	HOLLOWUI_API FHollowObjective Pick(const FHollowObjectiveFacts& Facts);

	/** Whether the step counts down (the tracker then redraws once a second). */
	HOLLOWUI_API bool IsTimed(EHollowObjectiveStep Step);

	/** m:ss, the visor's clock. */
	HOLLOWUI_API FText Clock(int32 Seconds);

	/** Credits as the hub shows them ("1,250 cr"). */
	HOLLOWUI_API FText Credits(int32 Amount);
}

/** Where a waypoint goes on the visor. */
struct HOLLOWUI_API FHollowWaypointPlacement
{
	/** Centre of the marker, in the visor's space (origin top left). */
	FVector2D Position = FVector2D::ZeroVector;
	/** The edge arrow's direction in degrees, clockwise from up (only off screen). */
	float ArrowDegrees = 0.f;
	/** The place itself is in view: no arrow. */
	bool bOnScreen = false;
};

/** UX-01: screen math of the waypoints (pure, tested). */
namespace HollowWaypointMath
{
	/**
	 * The direction from the view's centre to Target on screen (x right, y down), unit length. A point straight
	 * behind the view or on its axis points down, so its arrow never flips about.
	 */
	HOLLOWUI_API FVector2D ScreenDirection(const FVector& ViewLocation, const FRotator& ViewRotation, const FVector& Target);

	/**
	 * Target on a Size visor seen from a view with a horizontal field of view of HorizontalFovDegrees (the engine's
	 * default, the width keeps its angle): false if it lies behind the view (or on its plane).
	 */
	HOLLOWUI_API bool Project(const FVector& ViewLocation, const FRotator& ViewRotation, float HorizontalFovDegrees, const FVector2D& Size,
		const FVector& Target, FVector2D& OutPosition);

	/**
	 * Where a waypoint goes on a Size visor: at its projection if the place projected in front of the view and lies
	 * Margin inside the edges; else on that inset edge toward Direction (the projection's own direction when it
	 * projected), with an arrow pointing there.
	 */
	HOLLOWUI_API FHollowWaypointPlacement Place(const FVector2D& Size, float Margin, bool bProjected, const FVector2D& Projected, const FVector2D& Direction);

	/** Whole metres between two points, as the marker shows them. */
	/**
	 * UI-02 (ADR-0052): Placement moved off the visor's readouts (KeepOut, in the visor's space; Radius: the marker's
	 * half size). An edge arrow slides along its edge, a marker in view steps just beside the readout it would cover;
	 * either stays inside the Margin frame. Unchanged where no free place is found.
	 */
	HOLLOWUI_API FHollowWaypointPlacement AvoidKeepOut(const FHollowWaypointPlacement& Placement, const FVector2D& Size, float Margin, float Radius,
		TConstArrayView<FBox2D> KeepOut);

	HOLLOWUI_API int32 Metres(const FVector& From, const FVector& To);
}
