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

/** One player's meta profile, owned by their machine (ADR-0005). The vertical slice keeps credits and a loadout. */
USTRUCT(BlueprintType)
struct HOLLOWECONOMY_API FHollowProfile
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	int32 Credits = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	TArray<FHollowLoadoutSlot> Loadout;

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	FHollowRaidStats Stats;
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
};

/** Pure rules of the profile and its raid escrow (ADR-0005, ADR-0016). Tested by Hollow.Economy.Profile.Escrow. */
namespace HollowProfileRules
{
	/** Applies a resolved delta: credits, lost loadout items, counters. */
	HOLLOWECONOMY_API void Apply(FHollowProfile& Profile, const FHollowProfileDelta& Delta, EHollowRaidOutcome Outcome);

	/**
	 * Starts the escrow of RaidId with a snapshot of the loadout. Nothing happens for the raid already in escrow; a
	 * different unresolved raid is first resolved as Abandoned (left without a result). Returns that report, if any.
	 */
	HOLLOWECONOMY_API TOptional<FHollowRaidReport> BeginRaid(FHollowProfileState& State, const FGuid& RaidId, const FDateTime& NowUtc,
		const FHollowItemCatalog& Catalog);

	/** Resolves the escrow with the host's result; a result for another raid (or without escrow) is ignored. */
	HOLLOWECONOMY_API TOptional<FHollowRaidReport> ApplyResult(FHollowProfileState& State, const FHollowRaidResult& Result, const FHollowItemCatalog& Catalog);

	/** Resolves the escrow without a host (SignalLost, Abandoned): no loot. */
	HOLLOWECONOMY_API TOptional<FHollowRaidReport> ResolveEscrow(FHollowProfileState& State, EHollowRaidOutcome Outcome, const FDateTime& NowUtc,
		const FHollowItemCatalog& Catalog);

	/** Items whose catalog the rules need for this state and result. */
	HOLLOWECONOMY_API TArray<FPrimaryAssetId> ItemsToCatalog(const FHollowProfileState& State, const FHollowRaidResult* Result = nullptr);
}
