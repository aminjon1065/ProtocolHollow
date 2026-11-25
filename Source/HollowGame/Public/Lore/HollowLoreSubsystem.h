#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "UObject/PrimaryAssetId.h"

#include "HollowLoreSubsystem.generated.h"

class AController;

/**
 * The crew logs of a raid on the server (NAR-01b, ADR-0050): who in the squad knows which logs (each player's
 * loadout says it) and which log a downloaded datapad gives the squad (HollowLoreRules::PickEntry, from the sector's
 * logs and seed). The log goes into the raid's replicated found logs; every machine keeps it in its own profile.
 */
UCLASS()
class HOLLOWGAME_API UHollowLoreSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UHollowLoreSubsystem* Get(const UWorld* World);

	/** Server: the crew logs Player has found (its loadout snapshot, trusted like the rest of it). */
	void SetKnownLore(AController* Player, const TArray<FPrimaryAssetId>& Known);

	/**
	 * Server: a datapad of this raid was downloaded by Downloader. Picks its log, adds it to the raid's found logs and
	 * returns it; invalid (nothing changes) without a raid, a sector, or a log of the sector left.
	 */
	FPrimaryAssetId Download(const AActor* Downloader);

	/** The logs of a sector Game Feature, by number (loads the definitions on first use). */
	TArray<FPrimaryAssetId> GetSectorLore(const FString& SectorFeature) const;

private:
	/** Each squad member's known logs, by controller (gone ones are skipped). */
	TArray<TPair<TWeakObjectPtr<AController>, TArray<FPrimaryAssetId>>> Known;
};
