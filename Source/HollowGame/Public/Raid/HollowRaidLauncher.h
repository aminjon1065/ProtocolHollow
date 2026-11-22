#pragma once

#include "Containers/UnrealString.h"
#include "Misc/Optional.h"
#include "UObject/PrimaryAssetId.h"

class UWorld;

/**
 * Sends the session into a raid (S4-06): the menu's "Start raid" and the console's Hollow.Raid.Start take this one path.
 * The host travels everyone (seamless travel) to UHollowSectorSettings::RaidMap with the sector seed and the contract,
 * which picks the sector (ADR-0041); the squad comes back to the hub with the same contract chosen.
 */
namespace HollowRaidLauncher
{
	/** The travel URL option that names the raid's contract (ADR-0032). */
	inline constexpr const TCHAR* ContractOption = TEXT("Contract");

	/** Whether Package (a long package name) is the hub map (UHollowNetworkSettings::HubMap). */
	HOLLOWGAME_API bool IsHubMap(const FString& Package);

	/** Whether World is the hub map (any machine). */
	HOLLOWGAME_API bool IsInHub(const UWorld* World);

	/** Whether this machine may send the squad into a raid now: it hosts (solo or co-op) and stands in the hub. */
	HOLLOWGAME_API bool CanStartRaid(const UWorld* World);

	/**
	 * Travels the session to the raid map with Seed (a random one if unset) and the hub's selected contract. False on a
	 * client or without a raid map.
	 */
	HOLLOWGAME_API bool Travel(UWorld* World, TOptional<int32> Seed = {});

	/** The raid map's travel URL for SectorSeed and the hub's selected contract (empty without a raid map). */
	HOLLOWGAME_API FString MakeTravelUrl(const UWorld* World, int32 SectorSeed);

	/** The hub map's travel URL back from a raid on Contract: the hub keeps the squad's choice (empty without a hub map). */
	HOLLOWGAME_API FString MakeHubUrl(const FPrimaryAssetId& Contract);

	/**
	 * The contract of travel Options (ContractOption): the named contract, else UHollowSectorSettings::DefaultContract
	 * (no option: a raid map opened directly, Hollow.Raid.Start away from the hub; an unknown one is logged).
	 */
	HOLLOWGAME_API FPrimaryAssetId ContractFromOptions(const FString& Options);
}
