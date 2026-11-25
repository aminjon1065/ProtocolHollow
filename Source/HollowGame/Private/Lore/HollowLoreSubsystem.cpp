#include "Lore/HollowLoreSubsystem.h"

#include "Assembly/HollowSectorStateComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "HollowGameLog.h"
#include "Lore/HollowLoreEntryDefinition.h"
#include "Lore/HollowLoreRules.h"
#include "Raid/HollowRaidStateComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowLoreSubsystem)

UHollowLoreSubsystem* UHollowLoreSubsystem::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UHollowLoreSubsystem>() : nullptr;
}

void UHollowLoreSubsystem::SetKnownLore(AController* Player, const TArray<FPrimaryAssetId>& InKnown)
{
	if (!Player)
	{
		return;
	}
	Known.RemoveAll([Player](const TPair<TWeakObjectPtr<AController>, TArray<FPrimaryAssetId>>& Entry) { return !Entry.Key.IsValid() || Entry.Key == Player; });
	Known.Emplace(Player, InKnown);
}

TArray<FPrimaryAssetId> UHollowLoreSubsystem::GetSectorLore(const FString& SectorFeature) const
{
	TArray<FPrimaryAssetId> Ids;
	for (const UHollowLoreEntryDefinition* Entry : UHollowLoreEntryDefinition::LoadAll())
	{
		if (Entry->SectorFeature == SectorFeature)
		{
			Ids.Add(Entry->GetPrimaryAssetId());
		}
	}
	return Ids;
}

FPrimaryAssetId UHollowLoreSubsystem::Download(const AActor* Downloader)
{
	UWorld* World = GetWorld();
	UHollowRaidStateComponent* Raid = UHollowRaidStateComponent::Get(World);
	const UHollowSectorStateComponent* Sector = UHollowSectorStateComponent::Get(World);
	if (!World || World->GetNetMode() == NM_Client || !Raid || !Sector)
	{
		UE_LOG(LogHollowGame, Warning, TEXT("Crew log: %s downloaded a datapad outside a raid's server; nothing given"), *GetNameSafe(Downloader));
		return FPrimaryAssetId();
	}
	const FString SectorFeature = Sector->GetManifest().SectorPlugin.ToString();
	TArray<FHollowLoreCandidate> Candidates;
	for (const UHollowLoreEntryDefinition* Entry : UHollowLoreEntryDefinition::LoadAll())
	{
		if (Entry->SectorFeature == SectorFeature)
		{
			Candidates.Add({Entry->GetPrimaryAssetId(), Entry->Number});
		}
	}
	TArray<TArray<FPrimaryAssetId>> Squad;
	for (const TPair<TWeakObjectPtr<AController>, TArray<FPrimaryAssetId>>& Player : Known)
	{
		if (Player.Key.IsValid())
		{
			Squad.Add(Player.Value);
		}
	}
	const FPrimaryAssetId Picked = HollowLoreRules::PickEntry(Candidates, Raid->GetLoreFound(), Squad, Sector->GetManifest().Seed);
	UE_LOG(LogHollowGame, Log, TEXT("Crew log: %s downloads %s (sector %s, %d of its logs, %d known lists)"), *GetNameSafe(Downloader),
		Picked.IsValid() ? *Picked.ToString() : TEXT("nothing: every log of the sector was given"), *SectorFeature, Candidates.Num(), Squad.Num());
	if (Picked.IsValid())
	{
		Raid->AddLoreFound(Picked);
	}
	return Picked;
}
