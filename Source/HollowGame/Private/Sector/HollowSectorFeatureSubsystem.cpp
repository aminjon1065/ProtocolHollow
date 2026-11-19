#include "Sector/HollowSectorFeatureSubsystem.h"

#include "Data/HollowBiomeDefinition.h"
#include "Data/HollowRoomDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "GameFeaturePluginOperationResult.h"
#include "GameFeaturesSubsystem.h"
#include "GameFeatureTypes.h"
#include "HollowGameLog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowSectorFeatureSubsystem)

UHollowSectorFeatureSubsystem* UHollowSectorFeatureSubsystem::Get()
{
	return GEngine ? GEngine->GetEngineSubsystem<UHollowSectorFeatureSubsystem>() : nullptr;
}

bool UHollowSectorFeatureSubsystem::ResolveURL(const FString& PluginName, FString& OutURL) const
{
	if (UGameFeaturesSubsystem::Get().GetPluginURLByName(PluginName, OutURL))
	{
		return true;
	}
	UE_LOG(LogHollowGame, Error, TEXT("Sector plugin %s is not a known Game Feature (missing or disabled in the .uproject)."), *PluginName);
	return false;
}

void UHollowSectorFeatureSubsystem::ActivateSector(const FString& PluginName, FOnHollowSectorChanged OnDone)
{
	FString URL;
	if (!ResolveURL(PluginName, URL))
	{
		OnDone.ExecuteIfBound(false);
		return;
	}

	++References.FindOrAdd(PluginName);
	UGameFeaturesSubsystem::Get().LoadAndActivateGameFeaturePlugin(URL, FGameFeaturePluginLoadComplete::CreateWeakLambda(this,
		[this, PluginName, OnDone](const UE::GameFeatures::FResult& Result)
		{
			if (Result.HasError())
			{
				UE_LOG(LogHollowGame, Error, TEXT("Sector %s failed to activate: %s"), *PluginName, *Result.GetError());
				if (int32* Count = References.Find(PluginName); Count && --(*Count) <= 0)
				{
					References.Remove(PluginName);
				}
			}
			else
			{
				UE_LOG(LogHollowGame, Log, TEXT("Sector activated: %s"), *DescribeSector(PluginName));
			}
			OnDone.ExecuteIfBound(!Result.HasError());
		}));
}

void UHollowSectorFeatureSubsystem::DeactivateSector(const FString& PluginName, FOnHollowSectorChanged OnDone)
{
	int32* Count = References.Find(PluginName);
	FString URL;
	if (!Count || !ResolveURL(PluginName, URL))
	{
		UE_LOG(LogHollowGame, Warning, TEXT("Sector %s is not active here; nothing to deactivate."), *PluginName);
		OnDone.ExecuteIfBound(false);
		return;
	}
	if (--(*Count) > 0)
	{
		OnDone.ExecuteIfBound(true);   // still used by another game instance in this process
		return;
	}

	References.Remove(PluginName);
	UGameFeaturesSubsystem::Get().UnloadGameFeaturePlugin(URL, FGameFeaturePluginUnloadComplete::CreateWeakLambda(this,
		[this, PluginName, OnDone](const UE::GameFeatures::FResult& Result)
		{
			if (Result.HasError())
			{
				UE_LOG(LogHollowGame, Error, TEXT("Sector %s failed to deactivate: %s"), *PluginName, *Result.GetError());
			}
			else
			{
				UE_LOG(LogHollowGame, Log, TEXT("Sector deactivated: %s"), *DescribeSector(PluginName));
			}
			OnDone.ExecuteIfBound(!Result.HasError());
		}), /*bKeepRegistered=*/true);
}

bool UHollowSectorFeatureSubsystem::IsSectorActive(const FString& PluginName) const
{
	return UGameFeaturesSubsystem::Get().IsGameFeaturePluginActiveByName(PluginName);
}

FString UHollowSectorFeatureSubsystem::DescribeSector(const FString& PluginName) const
{
	FString URL;
	const FString State = UGameFeaturesSubsystem::Get().GetPluginURLByName(PluginName, URL)
		? UE::GameFeatures::ToString(UGameFeaturesSubsystem::Get().GetPluginState(URL))
		: FString(TEXT("Unknown"));
	const int32* Count = References.Find(PluginName);
	return FString::Printf(TEXT("%s state=%s references=%d rooms=%d"), *PluginName, *State, Count ? *Count : 0, GetSectorRooms(PluginName).Num());
}

TArray<FPrimaryAssetId> UHollowSectorFeatureSubsystem::GetSectorRooms(const FString& PluginName)
{
	return GetSectorAssets(PluginName, UHollowRoomDefinition::AssetType);
}

FPrimaryAssetId UHollowSectorFeatureSubsystem::GetSectorBiome(const FString& PluginName)
{
	const TArray<FPrimaryAssetId> Biomes = GetSectorAssets(PluginName, UHollowBiomeDefinition::AssetType);
	return Biomes.IsEmpty() ? FPrimaryAssetId() : Biomes[0];
}

TArray<FPrimaryAssetId> UHollowSectorFeatureSubsystem::GetSectorAssets(const FString& PluginName, const FPrimaryAssetType& Type)
{
	TArray<FPrimaryAssetId> Found;
	if (!UAssetManager::IsInitialized())
	{
		return Found;
	}

	const UAssetManager& AssetManager = UAssetManager::Get();
	const FString MountRoot = FString::Printf(TEXT("/%s/"), *PluginName);
	TArray<FPrimaryAssetId> All;
	AssetManager.GetPrimaryAssetIdList(Type, All);
	for (const FPrimaryAssetId& Id : All)
	{
		if (AssetManager.GetPrimaryAssetPath(Id).GetLongPackageName().StartsWith(MountRoot))
		{
			Found.Add(Id);
		}
	}
	Found.Sort([](const FPrimaryAssetId& A, const FPrimaryAssetId& B)
	{
		return A.ToString().Compare(B.ToString(), ESearchCase::CaseSensitive) < 0;
	});
	return Found;
}
