#pragma once

#include "Extraction/HollowRaidTypes.h"
#include "Misc/DateTime.h"

#include "HollowProfileTypes.generated.h"

struct FHollowItemCatalog;

/** Career counters (debrief, later the hub). */
USTRUCT(BlueprintType)
struct HOLLOWECONOMY_API FHollowRaidStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	int32 Raids = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	int32 Extractions = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	int32 Deaths = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	int32 CoresDelivered = 0;
};

/** A player class's loadout while the player plays another class (ADR-0034). */
USTRUCT(BlueprintType)
struct HOLLOWECONOMY_API FHollowClassLoadout
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	FPrimaryAssetId ClassId;

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	TArray<FHollowLoadoutSlot> Loadout;
};

/** One player's meta profile, owned by their machine (ADR-0005): credits, the loadout taken into raids, the stash. */
USTRUCT(BlueprintType)
struct HOLLOWECONOMY_API FHollowProfile
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	int32 Credits = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	TArray<FHollowLoadoutSlot> Loadout;

	/**
	 * What the player owns outside the loadout (ADR-0031): bought, crafted, taken off the loadout. Never at risk in a
	 * raid (only the loadout goes into escrow). Stacks of at most the item's MaxStack, one per stash slot.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	TArray<FHollowItemStack> Stash;

	/** Ship upgrades bought (ADR-0032), each once and for good (UHollowShipUpgradeDefinition ids). */
	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	TArray<FPrimaryAssetId> Upgrades;

	/**
	 * The class the player plays (ADR-0034, a UHollowClassDefinition id): Loadout is this class's. Profiles from before
	 * S6-01 name none and play the default class with the loadout they have.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	FPrimaryAssetId ClassId;

	/** The loadouts of the other classes played before (a class never played gets its starting loadout when picked). */
	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	TArray<FHollowClassLoadout> ClassLoadouts;

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	FHollowRaidStats Stats;

	/**
	 * Radio cues heard once for good (NAR-01a, ADR-0049: the hub briefing, the first sortie into each sector), by the
	 * cue's id in DA_RadioScript. Profiles from before NAR-01 have none: the player hears the briefing once more.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	TArray<FName> RadioHeard;

	/**
	 * Crew logs found for good (NAR-01b, ADR-0050: HollowLore ids, in the order found), saved the moment a datapad is
	 * downloaded whatever the raid's end. Profiles from before NAR-01 have none.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	TArray<FPrimaryAssetId> Lore;
};

/** Written before a raid so leaving it without a result is never free (ADR-0005): a snapshot of the loadout taken. */
USTRUCT(BlueprintType)
struct HOLLOWECONOMY_API FHollowRaidEscrow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	FGuid RaidId;

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	TArray<FHollowLoadoutSlot> Loadout;

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	FDateTime StartedUtc;

	/** NAR-01b: crew logs first found in this raid; their bonus is paid only if the player extracts. */
	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	TArray<FPrimaryAssetId> NewLore;

	bool IsActive() const { return RaidId.IsValid(); }
};

/** A resolved raid as the debrief shows it; kept until the player has seen it. */
USTRUCT(BlueprintType)
struct HOLLOWECONOMY_API FHollowRaidReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	FHollowRaidResult Result;

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	FHollowProfileDelta Delta;

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	int32 CreditsBefore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	int32 CreditsAfter = 0;
};

/** Everything a profile save holds (UHollowProfileSaveGame), so the escrow rules are testable without a save slot. */
USTRUCT(BlueprintType)
struct HOLLOWECONOMY_API FHollowProfileState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	FHollowProfile Profile;

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	FHollowRaidEscrow Escrow;

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	FHollowRaidReport PendingReport;

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	bool bReportPending = false;

	/** Ids of the last profile transactions applied (HollowCraftingRules::Apply, ADR-0031): one sent again changes nothing. */
	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	TArray<FGuid> RecentTransactions;
};

/** Pure rules of the profile and its raid escrow (ADR-0005, ADR-0016). Tested by Hollow.Economy.Profile.Escrow. */
namespace HollowProfileRules
{
	/**
	 * Applies a resolved delta: credits, spent and lost loadout items, spent insurance, the found loot into the stash
	 * (GP-01b; in stacks of the catalog's MaxStack, else as one stack each), counters.
	 */
	HOLLOWECONOMY_API void Apply(FHollowProfile& Profile, const FHollowProfileDelta& Delta, EHollowRaidOutcome Outcome,
		const FHollowItemCatalog* Catalog = nullptr);

	/**
	 * Starts the escrow of RaidId with a snapshot of the loadout. Nothing happens for the raid already in escrow; a
	 * different unresolved raid is first resolved as Abandoned (left without a result). Returns that report, if any.
	 */
	HOLLOWECONOMY_API TOptional<FHollowRaidReport> BeginRaid(FHollowProfileState& State, const FGuid& RaidId, const FDateTime& NowUtc,
		const FHollowItemCatalog& Catalog);

	/**
	 * Resolves the escrow with the host's result; a result for another raid (or without escrow) is ignored.
	 * CoreValuePercent: the owned ship upgrades' bonus on sold Aether cores (ADR-0032). StashSlots and SellPercent: the
	 * economy's (GP-01b: found loot without room in the stash is sold; 0 slots: no limit). LoreBonus: credits for each
	 * crew log first found in the raid, paid only if the player extracted (NAR-01b).
	 */
	HOLLOWECONOMY_API TOptional<FHollowRaidReport> ApplyResult(FHollowProfileState& State, const FHollowRaidResult& Result, const FHollowItemCatalog& Catalog,
		int32 CoreValuePercent = 0, int32 StashSlots = 0, int32 SellPercent = 0, int32 LoreBonus = 0);

	/** Resolves the escrow without a host (SignalLost, Abandoned): no loot. */
	HOLLOWECONOMY_API TOptional<FHollowRaidReport> ResolveEscrow(FHollowProfileState& State, EHollowRaidOutcome Outcome, const FDateTime& NowUtc,
		const FHollowItemCatalog& Catalog);

	/**
	 * NAR-01b: crew log Entry downloaded. New: kept for good, and if a raid is in escrow, its bonus waits for the raid's
	 * end (ApplyResult). False if the profile has it already or Entry names nothing.
	 */
	HOLLOWECONOMY_API bool AddLore(FHollowProfileState& State, const FPrimaryAssetId& Entry);

	/** Remembers radio cue CueId (NAR-01a); false if it was heard before or names nothing. */
	HOLLOWECONOMY_API bool MarkRadioHeard(FHollowProfile& Profile, FName CueId);

	/** Items whose catalog the rules need for this state and result. */
	HOLLOWECONOMY_API TArray<FPrimaryAssetId> ItemsToCatalog(const FHollowProfileState& State, const FHollowRaidResult* Result = nullptr);
}
