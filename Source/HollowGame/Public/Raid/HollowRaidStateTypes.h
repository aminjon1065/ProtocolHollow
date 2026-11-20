#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"

#include "HollowRaidStateTypes.generated.h"

/** Phases of a raid (Docs/Architecture/02 sec. 2.2, ADR-0016). */
UENUM(BlueprintType)
enum class EHollowRaidPhase : uint8
{
	/** Building the sector on every machine. */
	Assembling,
	/** Pawns are down; the raid starts in a moment. */
	Deploying,
	InRaid,
	/** The shuttle counts down to departure: someone alive is in the extraction zone. */
	Extraction,
	/** The raid is over; results are out, the squad returns to the hub soon. */
	Debrief
};

/** The raid as every machine sees it (UHollowRaidStateComponent on the game state, push model). */
USTRUCT(BlueprintType)
struct HOLLOWGAME_API FHollowRaidNetState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Raid")
	EHollowRaidPhase Phase = EHollowRaidPhase::Assembling;

	/** Identity of this raid on every machine: the profile escrow and the result carry it. */
	UPROPERTY(BlueprintReadOnly, Category = "Raid")
	FGuid RaidId;

	/** Server time the current timed phase ends (Deploying, Extraction = departure, Debrief = return); 0 if untimed. */
	UPROPERTY(BlueprintReadOnly, Category = "Raid")
	float PhaseEndServerTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Raid")
	uint8 LivingPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Raid")
	uint8 LivingInZone = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Raid")
	uint8 CoresInZone = 0;

	bool operator==(const FHollowRaidNetState&) const = default;
};
