#include "Tests/HollowGameTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Assembly/HollowSectorManifest.h"
#include "Assembly/HollowSectorStateComponent.h"
#include "Data/HollowBiomeDefinition.h"
#include "Framework/HollowGameState.h"
#include "GameFramework/PlayerController.h"
#include "Hub/HollowContractDefinition.h"
#include "Lore/HollowLoreEntryDefinition.h"
#include "Lore/HollowLoreRules.h"
#include "Lore/HollowLoreSubsystem.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif
#include "Raid/HollowRaidStateComponent.h"
#include "Tags/HollowGameplayTags.h"
#include "World/HollowDatapad.h"
#include "World/HollowLootCrate.h"

namespace HollowLoreTests
{
	// Content under test (create_story_assets.py, create_loot_assets.py).
	const TCHAR* const DatapadClass = TEXT("/Game/Hollow/Story/BP_Datapad.BP_Datapad_C");
	const TCHAR* const DocksBiome = TEXT("/HollowSector_Docks/Data/DA_Biome_Docks.DA_Biome_Docks");
	const TCHAR* const CathedralBiome = TEXT("/HollowSector_Cathedral/Data/DA_Biome_Cathedral.DA_Biome_Cathedral");
	const FString Docks(TEXT("HollowSector_Docks"));
	const FString Cathedral(TEXT("HollowSector_Cathedral"));
	constexpr int32 Logs = 12;
	constexpr int32 LogsPerSector = 6;
	constexpr int32 DatapadsPerRaid = 2;
	constexpr int32 DatapadMinRooms = 2;

	/** Test data. */
	constexpr int32 Seed = 4242;

	TArray<FHollowLoreCandidate> MakeCandidates(const int32 Count)
	{
		TArray<FHollowLoreCandidate> Candidates;
		for (int32 Number = 1; Number <= Count; ++Number)
		{
			Candidates.Add({FPrimaryAssetId(TEXT("HollowLore"), *FString::Printf(TEXT("Test_Lore_%02d"), Number)), Number});
		}
		return Candidates;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowLorePickTest, "Hollow.Game.Lore.Pick",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowLorePickTest::RunTest(const FString& Parameters)
{
	// NAR-01b (ADR-0050): a datapad gives the squad the log of its sector the fewest of them know; never one already
	// given this raid; the same inputs always give the same log.
	using namespace HollowLoreTests;
	const TArray<FHollowLoreCandidate> Six = MakeCandidates(LogsPerSector);
	const auto Id = [&Six](const int32 Number) { return Six[Number - 1].Id; };
	const FPrimaryAssetId First = HollowLoreRules::PickEntry(Six, {}, {}, Seed);
	TestTrue(TEXT("A log of the sector"), Six.ContainsByPredicate([&First](const FHollowLoreCandidate& C) { return C.Id == First; }));
	TestEqual(TEXT("Deterministic"), HollowLoreRules::PickEntry(Six, {}, {}, Seed), First);
	bool bSeedMatters = false;
	for (int32 Other = 0; Other < 16 && !bSeedMatters; ++Other)
	{
		bSeedMatters = HollowLoreRules::PickEntry(Six, {}, {}, Other) != First;
	}
	TestTrue(TEXT("The sector's seed decides between equals"), bSeedMatters);

	// One player knows 1, 2, 3; another 1, 2: the logs nobody knows go first, then the one only one knows, then the rest.
	const TArray<TArray<FPrimaryAssetId>> Squad = {{Id(1), Id(2), Id(3)}, {Id(1), Id(2)}};
	TArray<FPrimaryAssetId> Taken;
	for (int32 Pick = 0; Pick < LogsPerSector; ++Pick)
	{
		Taken.Add(HollowLoreRules::PickEntry(Six, Taken, Squad, Seed));
	}
	const auto Rank = [&Taken](const FPrimaryAssetId& Entry) { return Taken.IndexOfByKey(Entry); };
	TestTrue(TEXT("Every log once"), Taken.Num() == LogsPerSector && !Taken.Contains(FPrimaryAssetId())
		&& TSet<FPrimaryAssetId>(Taken).Num() == LogsPerSector);
	TestTrue(TEXT("Nobody's logs first (4, 5, 6)"), Rank(Id(4)) < 3 && Rank(Id(5)) < 3 && Rank(Id(6)) < 3);
	TestEqual(TEXT("Then the one only one player knows (3)"), Rank(Id(3)), 3);
	TestTrue(TEXT("The logs both know last (1, 2)"), Rank(Id(1)) >= 4 && Rank(Id(2)) >= 4);
	TestFalse(TEXT("Every log given: nothing left"), HollowLoreRules::PickEntry(Six, Taken, Squad, Seed).IsValid());
	TestFalse(TEXT("No candidate: nothing"), HollowLoreRules::PickEntry({}, {}, {}, Seed).IsValid());

	TestEqual(TEXT("CountKnown: found of all"), HollowLoreRules::CountKnown({Id(1), Id(2), Id(3)}, {Id(2), Id(5), Id(3)}), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowLoreDataTest, "Hollow.Game.Lore.Data",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowLoreDataTest::RunTest(const FString& Parameters)
{
	// The shipped crew logs: twelve, numbered 1..12, six in each sector a contract sends the squad to, each valid; the
	// datapad with its mesh and prompt; each sector's two datapads first among the Loot rules, the crates after them.
	using namespace HollowLoreTests;
	const TArray<const UHollowLoreEntryDefinition*> Entries = UHollowLoreEntryDefinition::LoadAll();
	TestEqual(TEXT("Twelve crew logs"), Entries.Num(), Logs);
	TMap<FString, int32> PerSector;
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const UHollowLoreEntryDefinition* Entry = Entries[Index];
		TestEqual(*FString::Printf(TEXT("%s: numbered in order"), *Entry->GetName()), Entry->Number, Index + 1);
		++PerSector.FindOrAdd(Entry->SectorFeature);
#if WITH_EDITOR
		FDataValidationContext Validation;
		TestTrue(*FString::Printf(TEXT("%s is valid"), *Entry->GetName()), Entry->IsDataValid(Validation) != EDataValidationResult::Invalid);
#endif
	}
	TestTrue(TEXT("Six in the Docks, six in the Cathedral"), PerSector.Num() == 2 && PerSector.FindRef(Docks) == LogsPerSector
		&& PerSector.FindRef(Cathedral) == LogsPerSector);
	for (const TCHAR* Contract : {TEXT("DA_Contract_Docks"), TEXT("DA_Contract_Cathedral")})
	{
		const UHollowContractDefinition* Definition = UHollowContractDefinition::Load(FPrimaryAssetId(UHollowContractDefinition::AssetType, Contract));
		TestTrue(FString(Contract) + TEXT(": its sector has its crew logs"), Definition && PerSector.FindRef(Definition->SectorFeature) == LogsPerSector);
	}

	const TSubclassOf<AHollowDatapad> Pad = LoadClass<AHollowDatapad>(nullptr, DatapadClass);
	if (!TestNotNull(TEXT("BP_Datapad"), Pad.Get()))
	{
		return false;
	}
	const AHollowDatapad* Default = Pad->GetDefaultObject<AHollowDatapad>();
	TestFalse(TEXT("BP_Datapad says what E does"), Default->GetInteractionInfo(nullptr).Prompt.IsEmpty());
	for (const TCHAR* Path : {DocksBiome, CathedralBiome})
	{
		const UHollowBiomeDefinition* Biome = LoadObject<UHollowBiomeDefinition>(nullptr, Path);
		if (!TestNotNull(Path, Biome))
		{
			continue;
		}
		TArray<const FHollowSocketActorRule*> Loot;
		for (const FHollowSocketActorRule& Rule : Biome->SocketActors)
		{
			if (Rule.SocketType == HollowTags::Socket_Loot)
			{
				Loot.Add(&Rule);
			}
		}
		TestTrue(FString(Path) + TEXT(": two datapads a raid, two rooms or more from the airlock, first"), Loot.Num() >= 2
			&& Loot[0]->ActorClass.LoadSynchronous() == Pad.Get() && Loot[0]->MaxPerSector == DatapadsPerRaid && Loot[0]->MinRoomDistance == DatapadMinRooms);
		TestTrue(FString(Path) + TEXT(": the crates take every Loot socket left"), Loot.Num() >= 2 && Loot[1]->MaxPerSector == 0
			&& Loot[1]->ActorClass.LoadSynchronous() && Loot[1]->ActorClass.LoadSynchronous()->IsChildOf(AHollowLootCrate::StaticClass()));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowLoreDatapadTest, "Hollow.Game.Lore.Datapad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowLoreDatapadTest::RunTest(const FString& Parameters)
{
	// The real BP_Datapad in a Docks raid on the server: downloaded, it gives the squad a Docks log nobody knows, the
	// raid's found logs carry it to every machine (OnLoreFound), the pad goes; the next pad gives another log.
	using namespace HollowLoreTests;
	FTestWorldWrapper Wrapper;
	UWorld* World = HollowGameTests::CreateWorld(*this, Wrapper);
	const TSubclassOf<AHollowDatapad> Pad = LoadClass<AHollowDatapad>(nullptr, DatapadClass);
	if (!World || !TestNotNull(TEXT("BP_Datapad"), Pad.Get()))
	{
		return false;
	}
	// The game state without BeginPlay: its sector loader does not assemble the manifest set below.
	AHollowGameState* GameState = World->SpawnActor<AHollowGameState>();
	World->SetGameState(GameState);
	UHollowSectorStateComponent* Sector = UHollowSectorStateComponent::Get(World);
	UHollowRaidStateComponent* Raid = UHollowRaidStateComponent::Get(World);
	UHollowLoreSubsystem* Lore = UHollowLoreSubsystem::Get(World);
	if (!TestTrue(TEXT("Sector and raid state, the lore subsystem"), Sector && Raid && Lore))
	{
		return false;
	}
	FHollowSectorManifest Manifest;
	Manifest.SectorPlugin = FName(*Docks);
	Manifest.Seed = Seed;
	Sector->SetManifest(Manifest);
	const TArray<FPrimaryAssetId> DocksLogs = Lore->GetSectorLore(Docks);
	if (!TestEqual(TEXT("Six Docks logs"), DocksLogs.Num(), LogsPerSector))
	{
		return false;
	}
	// One player knows the first four: the pads give the other two.
	APlayerController* Player = World->SpawnActor<APlayerController>();
	Lore->SetKnownLore(Player, {DocksLogs[0], DocksLogs[1], DocksLogs[2], DocksLogs[3]});
	TArray<TPair<FPrimaryAssetId, bool>> Heard;
	Raid->OnLoreFound.AddLambda([&Heard](const FPrimaryAssetId& Entry, const bool bNew) { Heard.Emplace(Entry, bNew); });

	APawn* Downloader = World->SpawnActor<APawn>();
	AHollowDatapad* First = World->SpawnActor<AHollowDatapad>(Pad, FTransform(FVector(100.0, 0.0, 6.0)));
	AHollowDatapad* Second = World->SpawnActor<AHollowDatapad>(Pad, FTransform(FVector(-100.0, 0.0, 6.0)));
	if (!TestTrue(TEXT("Two pads and a player"), First && Second && Downloader))
	{
		return false;
	}
	TestTrue(TEXT("A pad can be downloaded"), First->CanInteract(Downloader));
	First->Interact(Downloader);
	TestTrue(TEXT("Downloaded: gone, a Docks log nobody knew"), First->IsTaken() && !First->CanInteract(Downloader) && Raid->GetLoreFound().Num() == 1
		&& (Raid->GetLoreFound()[0] == DocksLogs[4] || Raid->GetLoreFound()[0] == DocksLogs[5]));
	TestTrue(TEXT("Every machine is told (new: no profile here)"), Heard.Num() == 1 && Heard[0].Key == Raid->GetLoreFound()[0] && Heard[0].Value);
	Second->Interact(Downloader);
	TestTrue(TEXT("The second pad: the other log nobody knew"), Raid->GetLoreFound().Num() == 2 && Raid->GetLoreFound().Contains(DocksLogs[4])
		&& Raid->GetLoreFound().Contains(DocksLogs[5]));
	First->Interact(Downloader);
	TestEqual(TEXT("A downloaded pad gives nothing more"), Raid->GetLoreFound().Num(), 2);
	return true;
}

#endif
