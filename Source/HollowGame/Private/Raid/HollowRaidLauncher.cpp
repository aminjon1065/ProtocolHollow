#include "Raid/HollowRaidLauncher.h"

#include "Engine/World.h"
#include "HollowGameLog.h"
#include "Hub/HollowContractDefinition.h"
#include "Hub/HollowHubStateComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sector/HollowSectorAuthorityComponent.h"
#include "Sector/HollowSectorSettings.h"
#include "Session/HollowNetworkSettings.h"

bool HollowRaidLauncher::IsHubMap(const FString& Package)
{
	const FString Hub = GetDefault<UHollowNetworkSettings>()->HubMap.GetLongPackageName();
	return !Hub.IsEmpty() && Package == Hub;
}

bool HollowRaidLauncher::IsInHub(const UWorld* World)
{
	return World && IsHubMap(UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()));
}

bool HollowRaidLauncher::CanStartRaid(const UWorld* World)
{
	return World && World->GetNetMode() != NM_Client && !World->IsInSeamlessTravel() && IsInHub(World);
}

bool HollowRaidLauncher::Travel(UWorld* World, const TOptional<int32> Seed)
{
	const FString RaidMap = GetDefault<UHollowSectorSettings>()->RaidMap.GetLongPackageName();
	if (!World || World->GetNetMode() == NM_Client || RaidMap.IsEmpty())
	{
		UE_LOG(LogHollowGame, Warning, TEXT("A raid is started by the host, with UHollowSectorSettings::RaidMap set."));
		return false;
	}
	const FString Url = MakeTravelUrl(World, Seed.IsSet() ? Seed.GetValue() : UHollowSectorAuthorityComponent::MakeRandomSeed());
	UE_LOG(LogHollowGame, Log, TEXT("Raid: travelling to %s"), *Url);
	return World->ServerTravel(Url);
}

FString HollowRaidLauncher::MakeTravelUrl(const UWorld* World, const int32 SectorSeed)
{
	const FString RaidMap = GetDefault<UHollowSectorSettings>()->RaidMap.GetLongPackageName();
	if (RaidMap.IsEmpty())
	{
		return FString();
	}
	FString Url = FString::Printf(TEXT("%s?Seed=%d"), *RaidMap, SectorSeed);
	// The contract the host picked in the hub goes along (ADR-0032).
	if (const UHollowHubStateComponent* Hub = UHollowHubStateComponent::Get(World); Hub && Hub->GetSelectedContract().IsValid())
	{
		Url += FString::Printf(TEXT("?%s=%s"), ContractOption, *Hub->GetSelectedContract().PrimaryAssetName.ToString());
	}
	return Url;
}

FString HollowRaidLauncher::MakeHubUrl(const FPrimaryAssetId& Contract)
{
	const FString Hub = GetDefault<UHollowNetworkSettings>()->HubMap.GetLongPackageName();
	if (Hub.IsEmpty() || !Contract.IsValid())
	{
		return Hub;
	}
	return FString::Printf(TEXT("%s?%s=%s"), *Hub, ContractOption, *Contract.PrimaryAssetName.ToString());
}

FPrimaryAssetId HollowRaidLauncher::ContractFromOptions(const FString& Options)
{
	const FPrimaryAssetId& Default = GetDefault<UHollowSectorSettings>()->DefaultContract;
	const FString Name = UGameplayStatics::ParseOption(Options, ContractOption);
	if (Name.IsEmpty())
	{
		return Default;
	}
	const FPrimaryAssetId Contract(UHollowContractDefinition::AssetType, FName(*Name));
	if (!UHollowContractDefinition::Exists(Contract))
	{
		UE_LOG(LogHollowGame, Warning, TEXT("Travel names contract %s, which does not exist: the default contract %s instead."), *Name, *Default.ToString());
		return Default;
	}
	return Contract;
}
