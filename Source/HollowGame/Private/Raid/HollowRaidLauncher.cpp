#include "Raid/HollowRaidLauncher.h"

#include "Engine/World.h"
#include "HollowGameLog.h"
#include "Sector/HollowSectorAuthorityComponent.h"
#include "Sector/HollowSectorSettings.h"
#include "Session/HollowNetworkSettings.h"

bool HollowRaidLauncher::IsHubMap(const FString& Package)
{
	const FString Hub = GetDefault<UHollowNetworkSettings>()->HubMap.GetLongPackageName();
	return !Hub.IsEmpty() && Package == Hub;
}

bool HollowRaidLauncher::CanStartRaid(const UWorld* World)
{
	return World && World->GetNetMode() != NM_Client && !World->IsInSeamlessTravel()
		&& IsHubMap(UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()));
}

bool HollowRaidLauncher::Travel(UWorld* World, const TOptional<int32> Seed)
{
	const FString RaidMap = GetDefault<UHollowSectorSettings>()->RaidMap.GetLongPackageName();
	if (!World || World->GetNetMode() == NM_Client || RaidMap.IsEmpty())
	{
		UE_LOG(LogHollowGame, Warning, TEXT("A raid is started by the host, with UHollowSectorSettings::RaidMap set."));
		return false;
	}
	const int32 SectorSeed = Seed.IsSet() ? Seed.GetValue() : UHollowSectorAuthorityComponent::MakeRandomSeed();
	const FString Url = FString::Printf(TEXT("%s?Seed=%d"), *RaidMap, SectorSeed);
	UE_LOG(LogHollowGame, Log, TEXT("Raid: travelling to %s"), *Url);
	return World->ServerTravel(Url);
}
