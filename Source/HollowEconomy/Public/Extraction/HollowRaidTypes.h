#pragma once

#include "Items/HollowItemTypes.h"
#include "Misc/Guid.h"

#include "HollowRaidTypes.generated.h"

/** How a raid ended for one player (ADR-0016). */
UENUM(BlueprintType)
enum class EHollowRaidOutcome : uint8
{
	None,
	/** Aboard the shuttle when it left: the loadout is kept, the squad's loot is sold. */
	Extracted,
	/** Killed in the raid: only insured slots are kept. */
	Dead,
	/** Alive but outside the zone when the shuttle left: lost like the dead. */
	MissingInAction,
	/** The host vanished: the loadout is kept, the loot is lost (ADR-0005). */
	SignalLost,
	/** This machine left the raid without a result (crash, alt-F4, quit): death rules (ADR-0005). */
	Abandoned
};

/** What the host tells one player at the end of a raid (ADR-0005: the host is authoritative for the raid). */
USTRUCT(BlueprintType)
struct HOLLOWECONOMY_API FHollowRaidResult
{
	GENERATED_BODY()

	/** The raid this result belongs to; a result for another raid than the escrow's is ignored. */
	UPROPERTY(BlueprintReadOnly, Category = "Raid")
	FGuid RaidId;

	UPROPERTY(BlueprintReadOnly, Category = "Raid")
	EHollowRaidOutcome Outcome = EHollowRaidOutcome::None;

	/** What the squad brought out (Aether cores in the zone at departure). Empty unless Extracted. */
	UPROPERTY(BlueprintReadOnly, Category = "Raid")
	TArray<FHollowItemStack> Loot;

	/** Loadout items this player used up in the raid (mines laid, fuel cells burnt; the host's inventory, ADR-0030). */
	UPROPERTY(BlueprintReadOnly, Category = "Raid")
	TArray<FHollowItemStack> Spent;

	UPROPERTY(BlueprintReadOnly, Category = "Raid")
	float RaidSeconds = 0.f;

	/** Players aboard the shuttle (debrief). */
	UPROPERTY(BlueprintReadOnly, Category = "Raid")
	int32 SquadExtracted = 0;

	/** Players in the squad (debrief). */
	UPROPERTY(BlueprintReadOnly, Category = "Raid")
	int32 SquadSize = 0;
};

/** Everything the resolver needs about one player's raid. Plain data. */
struct FHollowRaidContext
{
	TArray<FHollowLoadoutSlot> Loadout;
	TArray<FHollowItemStack> Loot;
	/** Loadout items used up in the raid (FHollowRaidResult::Spent; none without the host's result). */
	TArray<FHollowItemStack> Spent;
};

/** The change a raid makes to one profile (FHollowExtractionResolver), and its explanation for the debrief. */
USTRUCT(BlueprintType)
struct HOLLOWECONOMY_API FHollowProfileDelta
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Raid")
	int32 Credits = 0;

	/** Loadout items that stay with the player. */
	UPROPERTY(BlueprintReadOnly, Category = "Raid")
	TArray<FHollowItemStack> Kept;

	/** Loadout items taken from the profile. */
	UPROPERTY(BlueprintReadOnly, Category = "Raid")
	TArray<FHollowItemStack> Lost;

	/** Loot sold to the corporation for Credits. */
	UPROPERTY(BlueprintReadOnly, Category = "Raid")
	TArray<FHollowItemStack> Sold;

	/** Loadout items used up in the raid: taken from the profile whatever the outcome (never more than the loadout held). */
	UPROPERTY(BlueprintReadOnly, Category = "Raid")
	TArray<FHollowItemStack> Spent;
};
