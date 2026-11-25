#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Radio/HollowRadioScript.h"
#include "Raid/HollowRaidStateTypes.h"
#include "Resonance/HollowResonanceTypes.h"
#include "Templates/Function.h"

/** Everything the radio listens to, as this machine knows it (UHollowRadioModel gathers it from events). */
struct HOLLOWUI_API FHollowRadioFacts
{
	/** The hub: no raid, the shuttle's terminals are here. */
	bool bInHub = false;
	/** A raid with an id is under way. */
	bool bInRaid = false;
	FGuid RaidId;
	EHollowRaidPhase Phase = EHollowRaidPhase::Assembling;
	/** The raid's sector Game Feature (its manifest); none before the manifest arrives. */
	FName Sector;
	/** The player has a pawn to stand in (the hub briefing waits for it). */
	bool bHasBody = false;
	/** The player has a living body. */
	bool bAlive = false;
	uint8 LivingPlayers = 0;
	/** A core still in its vault came into the player's view this raid (a sensor; it stays true). */
	bool bCoreSighted = false;
	/** Some core was lifted this raid. */
	bool bCoreLifted = false;
	/** A core lies on an altar. */
	bool bRitual = false;
	/** The Echo-Stalker came into the player's view this raid (a sensor; it stays true). */
	bool bStalkerSighted = false;
	EHollowResonanceTier Tier = EHollowResonanceTier::Low;
	/** NAR-01b: crew logs new to this player downloaded this raid. */
	int32 NewLore = 0;

	bool operator==(const FHollowRadioFacts&) const = default;
};

/** What a call to FHollowRadioQueue::Advance changed. */
struct HOLLOWUI_API FHollowRadioEvents
{
	/** A transmission opened (its first line is on the air): the opening click. */
	bool bOpened = false;
	/** One closed (heard to its end or cut by a Critical one): the closing click. */
	bool bClosed = false;
	/** The line on the air changed (a new one, or none). */
	bool bLineChanged = false;
	/** Cues heard to their end (a Profile cue is remembered then). */
	TArray<int32> Finished;
	/** Cues that waited too long or were pushed out of a full queue. */
	TArray<int32> Dropped;
};

/** The line on the air. */
struct HOLLOWUI_API FHollowRadioPlaying
{
	int32 Cue = INDEX_NONE;
	int32 Variant = 0;
	int32 Line = 0;
	double LineEnds = 0.0;
};

/**
 * The radio's queue (NAR-01a, ADR-0049): pure rules, time passed in, so it is tested without a world. A fired trigger
 * queues its cue unless the cue's scope says it was heard; the most urgent waiting cue goes on the air once the last
 * one is over and CueGapSeconds have passed; its lines follow each other for as long as each takes to read. Only a
 * Critical cue cuts in: the one it cut waits at the front of the queue to go on from the line it was on.
 */
class HOLLOWUI_API FHollowRadioQueue
{
public:
	void SetScript(const UHollowRadioScript* InScript);
	const UHollowRadioScript* GetScript() const { return Script.Get(); }

	/**
	 * Trigger fired at Now in Sector during raid RaidId (none: the hub). HeardForGood tells whether a Profile cue is in
	 * the player's profile. bForce (development) ignores scopes and cooldowns. Returns the cue queued, or INDEX_NONE.
	 * A cue cut by a Critical one closes at once: call Advance.
	 */
	int32 Fire(const FGameplayTag& Trigger, FName Sector, const FGuid& RaidId, double Now, TFunctionRef<bool(FName)> HeardForGood, bool bForce = false);

	/** Moves the radio to Now. */
	FHollowRadioEvents Advance(double Now);

	/** When Advance has something to do next (a line ends, a cue may start, a waiting one goes stale). */
	TOptional<double> GetNextWake() const;

	const FHollowRadioPlaying* GetPlaying() const { return Playing.IsSet() ? &Playing.GetValue() : nullptr; }
	const FHollowRadioCue* GetPlayingCue() const;
	const FHollowRadioLine* GetPlayingLine() const;
	int32 GetNumQueued() const { return Queue.Num(); }
	bool IsQueued(int32 Cue) const;

	/** Drops everything waiting and on the air (a new world, a reset). */
	void Clear();

	/** Forgets which cues were heard this raid and when (development). */
	void Forget();

private:
	struct FRequest
	{
		int32 Cue = INDEX_NONE;
		int32 Variant = 0;
		/** The line to start with (a cue cut by a Critical one goes on where it was). */
		int32 Line = 0;
		double QueuedAt = 0.0;
		/** Queue order between equals: lower first. */
		double Order = 0.0;
	};

	const FHollowRadioCue* CueAt(int32 Index) const;
	EHollowRadioPriority PriorityOf(int32 Cue) const;
	/** Index into Queue of the cue to go on the air next. */
	int32 PickNext() const;
	void Start(const FRequest& Request, double At, FHollowRadioEvents& Events);
	int32 PickVariant(int32 Cue, const FGuid& RaidId) const;

	TWeakObjectPtr<const UHollowRadioScript> Script;
	TArray<FRequest> Queue;
	TOptional<FHollowRadioPlaying> Playing;
	/** When the last cue went off the air: the next waits CueGapSeconds after it (a Critical one does not). */
	TOptional<double> LastEnded;
	double NextOrder = 0.0;
	/** A Critical cue cut the one on the air (Fire): the next Advance tells it closed. */
	bool bCut = false;
	TArray<int32> PendingDropped;
	/** The raid the memory below belongs to. */
	FGuid Raid;
	/** Profile and Raid cues queued in this raid (a Profile one dropped or cut by travel comes back in the next world). */
	TSet<int32> Fired;
	/** When each Repeat cue last went on the air, and how often it did (its variants take turns). */
	TMap<int32, double> LastStarted;
	TMap<int32, int32> Plays;
};

namespace HollowRadioRules
{
	/** The triggers the change from Old to New fires, in a fixed order (pure, tested). */
	HOLLOWUI_API TArray<FGameplayTag> TriggersFor(const FHollowRadioFacts& Old, const FHollowRadioFacts& New);

	/** The sector the Sortie cue of these facts is for: the squad is coming down or in, and the manifest named it. */
	HOLLOWUI_API bool IsSortie(const FHollowRadioFacts& Facts);

	/** Whether the sensors still look (in the raid, on the ground, and the core or the stalker unseen). */
	HOLLOWUI_API bool ShouldSense(const FHollowRadioFacts& Facts);

	/**
	 * Whether Target counts as seen from a view at ViewLocation facing ViewDirection (unit): within MaxDistance and
	 * HalfAngleDegrees of the view's axis (the line of sight is the model's trace).
	 */
	HOLLOWUI_API bool IsInSight(const FVector& ViewLocation, const FVector& ViewDirection, const FVector& Target, float MaxDistance, float HalfAngleDegrees);
}
