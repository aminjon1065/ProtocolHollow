#include "Tests/HollowGameTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Collision/HollowCollisionChannels.h"
#include "Components/CapsuleComponent.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/AssetManager.h"
#include "EngineUtils.h"
#include "Framework/HollowGameState.h"
#include "Framework/HollowRaidGameMode.h"
#include "Framework/HollowPlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/ConfigCacheIni.h"
#include "Hub/HollowContractDefinition.h"
#include "Hub/HollowHubStateComponent.h"
#include "Hub/HollowHubTerminal.h"
#include "Hub/HollowHubTerminalSubsystem.h"
#include "Raid/HollowRaidFlowComponent.h"
#include "Raid/HollowRaidLauncher.h"
#include "Rules/HollowLightBudget.h"
#include "Sector/HollowSectorAuthorityComponent.h"
#include "Sector/HollowSectorFeatureSubsystem.h"
#include "Sector/HollowSectorSettings.h"
#include "Session/HollowNetworkSettings.h"
#include "Tags/HollowGameplayTags.h"
#include "Util/HollowEnumUtils.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace HollowHubTests
{
	const TCHAR* const SkiffMap = TEXT("/Game/Hollow/Maps/L_Hub_Skiff.L_Hub_Skiff");
	const FPrimaryAssetId Docks(TEXT("HollowContract"), TEXT("DA_Contract_Docks"));
	/** Sector II's contract (S7-04, ADR-0041). */
	const FPrimaryAssetId Cathedral(TEXT("HollowContract"), TEXT("DA_Contract_Cathedral"));
	const TCHAR* const DocksPlugin = TEXT("HollowSector_Docks");
	const TCHAR* const CathedralPlugin = TEXT("HollowSector_Cathedral");
	const FPrimaryAssetId CoreItem(TEXT("HollowItem"), TEXT("DA_Item_AetherCore"));
	const FPrimaryAssetId PurifiedItem(TEXT("HollowItem"), TEXT("DA_Item_PurifiedCore"));
	constexpr int32 ContractCount = 2;
	const TCHAR* const RaidMapName = TEXT("L_Raid_Dev");
	constexpr int32 PlayerStarts = 4;
	/** Where the test's player body waits, under the cabin. */
	constexpr double OutOfTheWay = 5000.0;
	/** Blocking flushes before the streamed level must be visible. */
	constexpr int32 StreamFlushes = 4;
	/** Eyes of a player standing at a terminal's use spot (the ExoFrame's camera height over the floor). */
	constexpr double EyeHeight = 160.0;
	/** A capsule is swept and placed this far over the floor (no contact with it). */
	constexpr double FloorClearance = 2.0;
	/** Height of a terminal's screen over the floor (create_hub_assets.py TERMINAL) and how far a look reaches. */
	constexpr double ScreenHeight = 105.0;
	constexpr double LookReach = 400.0;
	/** The cabin's diagonal (create_hub_assets.py CABIN): no light reaches further. */
	constexpr float CabinReach = 1850.f;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowHubTerminalsTest, "Hollow.Game.Hub.Terminals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowHubTerminalsTest::RunTest(const FString& Parameters)
{
	// ADR-0032: a terminal used by a player opens on that player's machine only (the server's Client_OpenTerminal); the
	// contract is the host's to pick (a guest is refused, a non-contract too); the hub starts on the default contract
	// and the raid's travel URL carries the choice.
	using namespace HollowHubTests;
	FTestWorldWrapper TestWorld;
	UWorld* World = HollowGameTests::CreateWorld(*this, TestWorld);
	if (!World)
	{
		return false;
	}
	AHollowGameState* GameState = World->SpawnActor<AHollowGameState>();
	GameState->DispatchBeginPlay();
	UHollowHubStateComponent* Hub = UHollowHubStateComponent::Get(World);
	if (!TestTrue(TEXT("The game state carries the hub state"), Hub && Hub == GameState->GetHubState()))
	{
		return false;
	}
	TestTrue(TEXT("The hub starts on the default contract"), Hub->GetSelectedContract() == GetDefault<UHollowSectorSettings>()->DefaultContract
		&& Hub->GetSelectedContract() == Docks);

	// The host (the listen server's own player) and a guest, each with an ExoFrame.
	AddExpectedMessage(TEXT("InputConfig is not set"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, /*Occurrences=*/0, /*IsRegex=*/false);
	AHollowExoFrameCharacter* HostBody = HollowGameTests::SpawnExoFrame(*this, World, FTransform(FVector(0.0, 0.0, 100.0)));
	AHollowExoFrameCharacter* GuestBody = HollowGameTests::SpawnExoFrame(*this, World, FTransform(FVector(0.0, 300.0, 100.0)));
	AHollowPlayerController* Host = HostBody ? HollowGameTests::PossessAsPlayer<AHollowPlayerController>(World, HostBody, /*bLocalPlayer=*/true) : nullptr;
	AHollowPlayerController* Guest = GuestBody ? HollowGameTests::PossessAsPlayer<AHollowPlayerController>(World, GuestBody) : nullptr;
	AHollowHubTerminal* Contracts = World->SpawnActor<AHollowHubTerminal>(FVector(500.0, 0.0, 0.0), FRotator(0.0, 180.0, 0.0));
	AHollowHubTerminal* Workbench = World->SpawnActor<AHollowHubTerminal>(FVector(500.0, 300.0, 0.0), FRotator(0.0, 180.0, 0.0));
	UHollowHubTerminalSubsystem* Terminals = UHollowHubTerminalSubsystem::Get(World);
	if (!TestTrue(TEXT("Players, terminals, the terminal subsystem"), Host && Guest && Contracts && Workbench && Terminals))
	{
		return false;
	}
	Contracts->Kind = EHollowTerminalKind::Contracts;
	Contracts->Prompt = FText::FromString(TEXT("Contracts"));
	Contracts->GuestPrompt = FText::FromString(TEXT("Contracts (host decides)"));
	Contracts->UseDistance = 110.f;
	Workbench->Kind = EHollowTerminalKind::Workbench;
	Workbench->Prompt = FText::FromString(TEXT("Workbench"));
	TArray<TPair<APlayerController*, AHollowHubTerminal*>> Opened;
	Terminals->OnTerminalOpened.AddLambda([&Opened](APlayerController* Player, AHollowHubTerminal* Terminal) { Opened.Emplace(Player, Terminal); });

	TestTrue(TEXT("The host's machine decides; the prompt is the host's"), AHollowHubTerminal::IsHostSide(HostBody)
		&& Contracts->GetInteractionInfo(HostBody).Prompt.EqualTo(Contracts->Prompt));
	TestTrue(TEXT("Its user stands in front of it"), FVector::Dist2D(Contracts->GetUseLocation(), FVector(390.0, 0.0, 0.0)) < 1.0);
	TestTrue(TEXT("Any player with a body may use a terminal"), Contracts->CanInteract(HostBody) && Workbench->CanInteract(GuestBody) && !Contracts->CanInteract(nullptr));
	Workbench->Interact(GuestBody);
	TestTrue(TEXT("The guest's workbench opens for the guest only"), Opened.Num() == 1 && Opened[0].Key == Guest && Opened[0].Value == Workbench
		&& Terminals->GetOpenTerminal(Guest) == Workbench && !Terminals->GetOpenTerminal(Host));
	Contracts->Interact(HostBody);
	TestTrue(TEXT("The host's contracts terminal for the host"), Opened.Num() == 2 && Opened[1].Key == Host && Terminals->GetOpenTerminal(Host) == Contracts
		&& Terminals->GetOpenTerminal(Guest) == Workbench);
	Workbench->Interact(HostBody);
	TestTrue(TEXT("Another terminal: the open one closes first"), Terminals->GetOpenTerminal(Host) == Workbench);
	Terminals->Close(Guest);
	TestFalse(TEXT("Closed"), Terminals->GetOpenTerminal(Guest) != nullptr);

	// Contracts: the host's to pick.
	AddExpectedMessage(TEXT("may not choose the contract"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1, /*IsRegex=*/false);
	AddExpectedMessage(TEXT("refused (not on the server, or no such contract)"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1, /*IsRegex=*/false);
	int32 Changes = 0;
	Hub->OnContractChanged.AddLambda([&Changes](const FPrimaryAssetId&) { ++Changes; });
	const FPrimaryAssetId NoContract(TEXT("HollowContract"), TEXT("Test_NoContract"));
	Guest->Server_SelectContract(Docks);
	Host->Server_SelectContract(NoContract);
	TestTrue(TEXT("A guest and a non-contract change nothing"), Hub->GetSelectedContract() == Docks && Changes == 0);
	Host->Server_SelectContract(Docks);
	TestTrue(TEXT("The host may pick it (unchanged: no event)"), Hub->GetSelectedContract() == Docks && Changes == 0);
	const FString Url = HollowRaidLauncher::MakeTravelUrl(World, 7);
	TestTrue(FString::Printf(TEXT("The raid's URL carries the contract (%s)"), *Url), Url.Contains(TEXT("?Seed=7")) && Url.Contains(TEXT("?Contract=DA_Contract_Docks")));
	Host->Server_SelectContract(Cathedral);
	const FString CathedralUrl = HollowRaidLauncher::MakeTravelUrl(World, 7);
	TestTrue(FString::Printf(TEXT("The host picks the Cathedral: the raid goes there (%s)"), *CathedralUrl), Hub->GetSelectedContract() == Cathedral && Changes == 1
		&& HollowRaidLauncher::ContractFromOptions(CathedralUrl.Mid(CathedralUrl.Find(TEXT("?")))) == Cathedral);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowHubDataTest, "Hollow.Game.Hub.Data",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowHubDataTest::RunTest(const FString& Parameters)
{
	// The shipped hub: L_Hub_Skiff is the hub map; the hub offers both sectors' contracts (S7-04, ADR-0041), each valid,
	// naming a sector Game Feature with a biome and rooms, its sector's name and what it pays; the Docks are the default.
	using namespace HollowHubTests;
	TestEqual(TEXT("The hub map is the Skiff"), GetDefault<UHollowNetworkSettings>()->HubMap.ToSoftObjectPath().ToString(), FString(SkiffMap));
	TestTrue(TEXT("The hub starts on the Docks"), GetDefault<UHollowSectorSettings>()->DefaultContract == Docks);
	TArray<FPrimaryAssetId> Listed;
	UAssetManager::Get().GetPrimaryAssetIdList(UHollowContractDefinition::AssetType, Listed);
	TestTrue(FString::Printf(TEXT("The hub lists the %d contracts (%d)"), ContractCount, Listed.Num()), Listed.Num() == ContractCount && Listed.Contains(Docks)
		&& Listed.Contains(Cathedral));
	TestTrue(TEXT("A contract id names a contract only"), UHollowContractDefinition::Exists(Docks) && UHollowContractDefinition::Exists(Cathedral)
		&& !UHollowContractDefinition::Exists(FPrimaryAssetId(TEXT("HollowItem"), TEXT("DA_Contract_Docks"))) && !UHollowContractDefinition::Load(CoreItem));
	FString RaidSector;
	TestFalse(TEXT("No configured raid sector is left: the contract picks it"),
		GConfig->GetString(TEXT("/Script/HollowGame.HollowSectorSettings"), TEXT("RaidSector"), RaidSector, GGameIni));

	const struct
	{
		FPrimaryAssetId Id;
		const TCHAR* Sector;
		TArray<FPrimaryAssetId> Rewards;
	} Expected[] = {{Docks, DocksPlugin, {CoreItem}}, {Cathedral, CathedralPlugin, {CoreItem, PurifiedItem}}};
	for (const auto& Each : Expected)
	{
		const UHollowContractDefinition* Contract = UHollowContractDefinition::Load(Each.Id);
		if (!TestNotNull(*Each.Id.ToString(), Contract))
		{
			continue;
		}
		TestEqual(FString::Printf(TEXT("%s: its sector"), *Each.Id.ToString()), Contract->SectorFeature, FString(Each.Sector));
		TestTrue(FString::Printf(TEXT("%s: a sector Game Feature with a biome and rooms"), *Each.Id.ToString()),
			UHollowSectorFeatureSubsystem::GetSectorBiome(Contract->SectorFeature).IsValid() && !UHollowSectorFeatureSubsystem::GetSectorRooms(Contract->SectorFeature).IsEmpty());
		TestTrue(FString::Printf(TEXT("%s: named, its sector named"), *Each.Id.ToString()), !Contract->DisplayName.IsEmpty() && !Contract->SectorName.IsEmpty());
		TestTrue(FString::Printf(TEXT("%s: pays with its items"), *Each.Id.ToString()), Contract->RewardItems == Each.Rewards);
#if WITH_EDITOR
		FDataValidationContext Validation;
		TestTrue(FString::Printf(TEXT("%s: valid"), *Each.Id.ToString()), Contract->IsDataValid(Validation) != EDataValidationResult::Invalid && Validation.GetNumErrors() == 0);
#endif
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowRaidContractSectorTest, "Hollow.Game.Raid.ContractSector",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowRaidContractSectorTest::RunTest(const FString& Parameters)
{
	// ADR-0041: the raid's travel URL names the contract and the contract picks the sector. A raid without one (the raid
	// map opened directly) or with an unknown one gets the default contract; the squad comes back to the hub with the
	// same contract chosen.
	using namespace HollowHubTests;
	const FPrimaryAssetId& Default = GetDefault<UHollowSectorSettings>()->DefaultContract;
	AddExpectedMessage(TEXT("which does not exist"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, /*Occurrences=*/2, /*IsRegex=*/false);
	TestTrue(TEXT("The URL's contract"), HollowRaidLauncher::ContractFromOptions(TEXT("?Seed=7?Contract=DA_Contract_Cathedral")) == Cathedral);
	TestTrue(TEXT("No contract: the default one"), HollowRaidLauncher::ContractFromOptions(TEXT("?Seed=7")) == Default && Default == Docks);
	TestTrue(TEXT("An unknown contract: the default one"), HollowRaidLauncher::ContractFromOptions(TEXT("?Contract=Test_NoContract")) == Default);

	const FString Hub = GetDefault<UHollowNetworkSettings>()->HubMap.GetLongPackageName();
	const FString HubUrl = HollowRaidLauncher::MakeHubUrl(Cathedral);
	TestTrue(FString::Printf(TEXT("Back to the hub with the contract (%s)"), *HubUrl), HubUrl.StartsWith(Hub + TEXT("?"))
		&& HollowRaidLauncher::ContractFromOptions(HubUrl.Mid(Hub.Len())) == Cathedral);
	TestEqual(TEXT("Without a contract: the hub map alone"), HollowRaidLauncher::MakeHubUrl(FPrimaryAssetId()), Hub);

	// The raid game mode's InitGame gives the contract to the raid flow and its sector to the authority.
	FTestWorldWrapper TestWorld;
	UWorld* World = HollowGameTests::CreateWorld(*this, TestWorld);
	AHollowRaidGameMode* Raid = World ? World->SpawnActor<AHollowRaidGameMode>() : nullptr;
	if (!TestNotNull(TEXT("A raid game mode"), Raid))
	{
		return false;
	}
	const struct
	{
		const TCHAR* Options;
		FPrimaryAssetId Contract;
		const TCHAR* Sector;
	} Cases[] = {{TEXT("?Seed=7?Contract=DA_Contract_Cathedral"), Cathedral, CathedralPlugin}, {TEXT("?Seed=7?Contract=DA_Contract_Docks"), Docks, DocksPlugin},
		{TEXT("?Seed=7"), Docks, DocksPlugin}, {TEXT("?Seed=7?Contract=Test_NoContract"), Docks, DocksPlugin}};
	for (const auto& Case : Cases)
	{
		FString Error;
		static_cast<AGameModeBase*>(Raid)->InitGame(RaidMapName, Case.Options, Error);
		TestTrue(FString::Printf(TEXT("%s: contract %s, sector %s (got %s, %s)"), Case.Options, *Case.Contract.ToString(), Case.Sector,
			*Raid->GetRaidFlow()->GetContract().ToString(), *Raid->GetSectorAuthority()->GetSectorPlugin()),
			Error.IsEmpty() && Raid->GetRaidFlow()->GetContract() == Case.Contract && Raid->GetSectorAuthority()->GetSectorPlugin() == Case.Sector);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowHubSkiffTest, "Hollow.Game.Hub.Skiff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowHubSkiffTest::RunTest(const FString& Parameters)
{
	// L_Hub_Skiff as built (create_hub_assets.py), streamed into a test world: four player starts, one terminal of each
	// kind; from every start a player walks to every terminal's use spot and finds it focusable there (its cabinet blocks
	// the Interaction channel at eye height); the light rig within the hub's DA_LightBudget limits.
	using namespace HollowHubTests;
	FTestWorldWrapper TestWorld;
	if (!TestWorld.CreateTestWorld(EWorldType::Game))
	{
		TestWorld.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = TestWorld.GetTestWorld();
	// A player's body, for its capsule, out of the cabin's way. Spawned first: a bare ExoFrame reports its camera socket
	// (SpawnExoFrame expects it), and so do earlier tests' bodies when loading the level purges them.
	const AHollowExoFrameCharacter* Body = HollowGameTests::SpawnExoFrame(*this, World, FTransform(FVector(0.0, 0.0, -OutOfTheWay)));
	if (!TestNotNull(TEXT("A player's body"), Body))
	{
		return false;
	}
	bool bLoaded = false;
	ULevelStreamingDynamic* Level = ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(World, TSoftObjectPtr<UWorld>(FSoftObjectPath(SkiffMap)),
		FTransform::Identity, bLoaded, TEXT("SkiffTest"));
	if (!TestTrue(TEXT("L_Hub_Skiff streams"), bLoaded && Level))
	{
		return false;
	}
	Level->bShouldBlockOnLoad = true;
	for (int32 Flush = 0; Flush < StreamFlushes && !Level->IsLevelVisible(); ++Flush)
	{
		World->FlushLevelStreaming(EFlushLevelStreamingType::Full);
	}
	if (!TestTrue(TEXT("...and is visible"), Level->IsLevelVisible()))
	{
		return false;
	}
	TArray<const APlayerStart*> Starts;
	TMap<EHollowTerminalKind, const AHollowHubTerminal*> Terminals;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		Starts.Add(*It);
	}
	for (TActorIterator<AHollowHubTerminal> It(World); It; ++It)
	{
		TestFalse(TEXT("One terminal per kind"), Terminals.Contains(It->GetKind()));
		Terminals.Add(It->GetKind(), *It);
	}
	TestEqual(TEXT("Four player starts"), Starts.Num(), PlayerStarts);
	TestEqual(TEXT("Contracts, workbench, locker, crew archive (NAR-01b)"), Terminals.Num(), 4);

	// Walk from every start to every terminal: the player's capsule, the pawns' collision.
	const float Radius = Body->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float HalfHeight = Body->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	const FCollisionQueryParams Query(SCENE_QUERY_STAT(HollowSkiffTest));
	for (const TPair<EHollowTerminalKind, const AHollowHubTerminal*>& Terminal : Terminals)
	{
		const FVector Use = Terminal.Value->GetUseLocation() + FVector(0.0, 0.0, HalfHeight + FloorClearance);
		const FString Kind = HollowEnumToString(Terminal.Key);
		TestTrue(Kind + TEXT(": its user stands in front of it"), FVector::Dist2D(Use, Terminal.Value->GetActorLocation()) > Radius);
		TestFalse(Kind + TEXT(": its use spot is free"), World->OverlapBlockingTestByChannel(Use, FQuat::Identity, ECC_Pawn, Capsule, Query));
		for (const APlayerStart* Start : Starts)
		{
			FHitResult Hit;
			const FVector From(Start->GetActorLocation().X, Start->GetActorLocation().Y, Use.Z);
			TestFalse(FString::Printf(TEXT("%s -> %s: nothing in the way (%s)"), *Start->GetName(), *Kind, *GetNameSafe(Hit.GetActor())),
				World->SweepSingleByChannel(Hit, From, Use, FQuat::Identity, ECC_Pawn, Capsule, Query));
		}
		FHitResult Focus;
		const FVector Eyes = Terminal.Value->GetUseLocation() + FVector(0.0, 0.0, EyeHeight);
		const FVector Screen = Terminal.Value->GetActorLocation() + FVector(0.0, 0.0, ScreenHeight);
		const FVector Ahead = Eyes + (Screen - Eyes).GetSafeNormal() * LookReach;   // the player looks at its screen
		const bool bHit = World->LineTraceSingleByChannel(Focus, Eyes, Ahead, HollowCollision::Interaction, Query);
		TestTrue(FString::Printf(TEXT("%s: focusable from its use spot (the trace meets %s)"), *Kind, *GetNameSafe(Focus.GetActor())),
			bHit && Focus.GetActor() == Terminal.Value);
	}

	// The light rig against the hub's limits (DA_LightBudget, Hollow.Room.Type.Hub).
	const UHollowLightBudgetConfig* Budget = UHollowLightBudgetConfig::Get();
	const FHollowRoomLightLimits* Limits = Budget ? Budget->FindLimits(FGameplayTagContainer(HollowTags::Room_Type_Hub)) : nullptr;
	const ULevel* Loaded = Level->GetLoadedLevel();
	if (TestNotNull(TEXT("The hub's light limits"), Limits) && TestNotNull(TEXT("The loaded level"), Loaded))
	{
		const TArray<FHollowLightInfo> Lights = HollowLightBudget::CollectLevel(*Loaded);
		const TArray<FString> Problems = HollowLightBudget::CheckRoom(Lights, *Limits, CabinReach);
		TestTrue(FString::Printf(TEXT("%d lights within the hub's budget%s%s"), Lights.Num(), Problems.IsEmpty() ? TEXT("") : TEXT(": "),
			*FString::Join(Problems, TEXT("; "))), Problems.IsEmpty() && !Lights.IsEmpty());
	}
	Level->SetIsRequestingUnloadAndRemoval(true);
	World->FlushLevelStreaming(EFlushLevelStreamingType::Full);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
