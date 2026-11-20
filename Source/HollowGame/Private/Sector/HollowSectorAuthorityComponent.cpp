#include "Sector/HollowSectorAuthorityComponent.h"

#include "Assembly/HollowSectorAssemblySubsystem.h"
#include "Assembly/HollowSectorStateComponent.h"
#include "Components/CapsuleComponent.h"
#include "Data/HollowBiomeDefinition.h"
#include "Data/HollowLevelMetrics.h"
#include "Data/HollowRoomDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Generation/HollowRandom.h"
#include "Generation/HollowSectorGenerator.h"
#include "Generation/HollowSectorGraph.h"
#include "Generation/HollowSectorTemplates.h"
#include "HollowGameLog.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/DateTime.h"
#include "Nav/HollowNavSettings.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Sector/HollowSectorFeatureSubsystem.h"
#include "Sector/HollowSectorSettings.h"
#include "Sector/HollowSectorTopologyBuilder.h"
#include "Sector/HollowSectorTopologySubsystem.h"
#include "Tags/HollowGameplayTags.h"
#include "TimerManager.h"
#include "Util/HollowEnumUtils.h"
#include "World/HollowSectorMarker.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowSectorAuthorityComponent)

#define LOCTEXT_NAMESPACE "HollowSectorAuthority"

namespace HollowSectorAuthorityPrivate
{
	const TCHAR* const SeedOption = TEXT("Seed");

	/** World location of the first socket of a type in a room, if it has one. */
	TOptional<FVector> SocketLocation(const FHollowSectorManifest& Manifest, const FHollowSectorLayout& Layout, const int32 RoomIndex, const FGameplayTag& Type)
	{
		const UHollowRoomDefinition* Room = Manifest.Rooms[RoomIndex].Room;
		const FHollowRoomSocket* Socket = Room->Sockets.FindByPredicate([&Type](const FHollowRoomSocket& S) { return S.SocketType == Type; });
		if (!Socket)
		{
			return {};
		}
		const FTransform RoomTransform = HollowSectorManifest::RoomTransform(Layout.Rooms[RoomIndex], FIntPoint(Room->Footprint.X, Room->Footprint.Y),
			*GetDefault<UHollowLevelMetrics>(), UHollowSectorAssemblySubsystem::GetSectorOrigin());
		return (Socket->Transform * RoomTransform).GetLocation();
	}

	int32 FindRole(const FHollowSectorLayout& Layout, const EHollowRoomRole Role)
	{
		return Layout.Rooms.IndexOfByPredicate([Role](const FHollowPlacedRoom& Room) { return Room.Role == Role; });
	}
}

UHollowSectorAuthorityComponent::UHollowSectorAuthorityComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

int32 UHollowSectorAuthorityComponent::MakeRandomSeed()
{
	return static_cast<int32>(HollowRandom::Mix(static_cast<uint64>(FDateTime::UtcNow().GetTicks())) & static_cast<uint64>(MAX_int32));
}

void UHollowSectorAuthorityComponent::BeginPlay()
{
	Super::BeginPlay();
	const AGameModeBase* GameMode = Cast<AGameModeBase>(GetOwner());
	UHollowSectorAssemblySubsystem* Assembly = GetWorld()->GetSubsystem<UHollowSectorAssemblySubsystem>();
	if (!GameMode || !Assembly)
	{
		return;
	}
	AssembledHandle = Assembly->OnAssembled.AddUObject(this, &ThisClass::HandleServerAssembled);

	const FString& Options = GameMode->OptionsString;
	const bool bHasSeed = UGameplayStatics::HasOption(Options, HollowSectorAuthorityPrivate::SeedOption);
	StartBuild(bHasSeed ? UGameplayStatics::GetIntOption(Options, HollowSectorAuthorityPrivate::SeedOption, 0) : MakeRandomSeed());
}

void UHollowSectorAuthorityComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Also reached from garbage collection of a torn-down world (a component's BeginDestroy ends play): the world may be gone.
	if (UWorld* World = GetWorld())
	{
		if (UHollowSectorAssemblySubsystem* Assembly = World->GetSubsystem<UHollowSectorAssemblySubsystem>())
		{
			Assembly->OnAssembled.Remove(AssembledHandle);
		}
		World->GetTimerManager().ClearTimer(ReadyTimer);
	}
	if (ContentHandle.IsValid())
	{
		ContentHandle->CancelHandle();
	}
	if (UAssetManager::IsInitialized() && !RoomIds.IsEmpty())
	{
		TArray<FPrimaryAssetId> Loaded = RoomIds;
		Loaded.Add(BiomeId);
		UAssetManager::Get().UnloadPrimaryAssets(Loaded);
	}
	Super::EndPlay(EndPlayReason);
}

void UHollowSectorAuthorityComponent::StartBuild(const int32 Seed)
{
	PendingSeed = Seed;
	ClearSector();
	if (bContentLoaded)
	{
		Generate();
	}
	else if (!ContentHandle.IsValid())
	{
		LoadContent();
	}
}

void UHollowSectorAuthorityComponent::LoadContent()
{
	const FString& Sector = GetDefault<UHollowSectorSettings>()->RaidSector;
	BiomeId = UHollowSectorFeatureSubsystem::GetSectorBiome(Sector);
	RoomIds = UHollowSectorFeatureSubsystem::GetSectorRooms(Sector);
	if (!BiomeId.IsValid() || RoomIds.IsEmpty())
	{
		UE_LOG(LogHollowGame, Error, TEXT("Sector %s has no biome or rooms (HollowSectorSettings.RaidSector, ADR-0012)."), *Sector);
		return;
	}
	TArray<FPrimaryAssetId> Ids = RoomIds;
	Ids.Add(BiomeId);
	ContentHandle = UAssetManager::Get().LoadPrimaryAssets(Ids, {}, FStreamableDelegate::CreateUObject(this, &ThisClass::HandleDefinitionsLoaded));
	if (!ContentHandle.IsValid())
	{
		HandleDefinitionsLoaded();   // everything was loaded already
	}
}

void UHollowSectorAuthorityComponent::HandleDefinitionsLoaded()
{
	const UAssetManager& AssetManager = UAssetManager::Get();
	Biome = AssetManager.GetPrimaryAssetObject<UHollowBiomeDefinition>(BiomeId);
	Rooms.Reset();
	for (const FPrimaryAssetId& Id : RoomIds)
	{
		if (UHollowRoomDefinition* Room = AssetManager.GetPrimaryAssetObject<UHollowRoomDefinition>(Id))
		{
			Rooms.Add(Room);
		}
	}
	if (!Biome)
	{
		UE_LOG(LogHollowGame, Error, TEXT("Biome %s failed to load."), *BiomeId.ToString());
		return;
	}

	// Gameplay actor classes of the biome, so spawning never loads synchronously.
	TArray<FSoftObjectPath> Classes;
	for (const FHollowSocketActorRule& Rule : Biome->SocketActors)
	{
		Classes.AddUnique(Rule.ActorClass.ToSoftObjectPath());
	}
	for (const TSoftClassPtr<AActor>& Class : Biome->GetConnectionActorClasses())
	{
		Classes.AddUnique(Class.ToSoftObjectPath());
	}
	Classes.RemoveAll([](const FSoftObjectPath& Path) { return Path.IsNull(); });
	ContentHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(MoveTemp(Classes), FStreamableDelegate::CreateUObject(this, &ThisClass::HandleClassesLoaded));
	if (!ContentHandle.IsValid())
	{
		HandleClassesLoaded();
	}
}

void UHollowSectorAuthorityComponent::HandleClassesLoaded()
{
	bContentLoaded = true;
	Generate();
}

void UHollowSectorAuthorityComponent::Generate()
{
	FHollowSectorGenInput Input;
	Input.Seed = PendingSeed;
	Input.Rules = Biome->GenRules;
	TArray<const UHollowRoomDefinition*> Pool;
	for (const UHollowRoomDefinition* Room : Rooms)
	{
		Pool.Add(Room);
	}
	Input.Templates = HollowSectorTemplates::MakeTemplates(Pool, Biome->GenRules);

	FHollowSectorLayout Layout;
	if (FHollowSectorGenerator::Generate(Input, Layout) != EHollowGenResult::Success)
	{
		UE_LOG(LogHollowGame, Error, TEXT("Sector seed %d did not generate; the raid has no sector."), PendingSeed);
		return;
	}

	TArray<UHollowRoomDefinition*> Definitions;
	for (const FString& Id : Layout.TemplateIds)
	{
		TObjectPtr<UHollowRoomDefinition>* Room = Rooms.FindByPredicate([&Id](const UHollowRoomDefinition* R) { return R->GetPrimaryAssetId().ToString() == Id; });
		Definitions.Add(Room ? Room->Get() : nullptr);
	}
	FHollowSectorManifest Manifest;
	FString Error;
	if (!HollowSectorManifest::Build(Layout, Definitions, Biome, FName(GetDefault<UHollowSectorSettings>()->RaidSector), ++BuildCounter, Manifest, Error))
	{
		UE_LOG(LogHollowGame, Error, TEXT("Sector seed %d: %s"), PendingSeed, *Error);
		return;
	}

	Reported.Reset();
	bServerAssembled = false;
	bNavigationReady = false;
	GetWorld()->GetTimerManager().SetTimer(ReadyTimer, this, &ThisClass::HandleReadyTimeout, GetDefault<UHollowSectorSettings>()->ReadyTimeoutSeconds, /*bLoop=*/false);
	UE_LOG(LogHollowGame, Log, TEXT("Sector build %d: seed %d, %d rooms in %d attempts, hash %08X"), Manifest.BuildId, PendingSeed,
		Layout.Rooms.Num(), Layout.Attempts, Layout.LayoutHash);
	GetSectorState()->SetManifest(Manifest);   // the server's own loader assembles it like every client
}

void UHollowSectorAuthorityComponent::HandleServerAssembled(const int32 BuildId, const uint32 LocalHash)
{
	const FHollowSectorManifest& Manifest = GetSectorState()->GetManifest();
	if (BuildId != Manifest.BuildId)
	{
		return;
	}
	if (LocalHash != Manifest.LayoutHash)
	{
		UE_LOG(LogHollowGame, Error, TEXT("Server rebuilt its own sector build %d with hash %08X instead of %08X."), BuildId, LocalHash, Manifest.LayoutHash);
		return;
	}
	bServerAssembled = true;
	SpawnSectorActors();
	BuildNavigation();
	if (UHollowSectorTopologySubsystem* Topology = UHollowSectorTopologySubsystem::Get(this))
	{
		// The AI's view of the sector (heat, vents, the director's stalker, ADR-0019); server only.
		const UHollowSectorAssemblySubsystem* Assembly = GetWorld()->GetSubsystem<UHollowSectorAssemblySubsystem>();
		Topology->SetTopology(HollowSectorTopologyBuilder::Build(Assembly->GetManifest(), Assembly->GetLayout(), *GetDefault<UHollowLevelMetrics>(),
			UHollowSectorAssemblySubsystem::GetSectorOrigin()));
	}
	TryFinish();
}

void UHollowSectorAuthorityComponent::SpawnSectorActors()
{
	UWorld* World = GetWorld();
	const UHollowSectorAssemblySubsystem* Assembly = World->GetSubsystem<UHollowSectorAssemblySubsystem>();
	const FHollowSectorManifest& Manifest = Assembly->GetManifest();
	const FHollowSectorLayout& Layout = Assembly->GetLayout();
	const UHollowLevelMetrics* Metrics = GetDefault<UHollowLevelMetrics>();
	const FVector Origin = UHollowSectorAssemblySubsystem::GetSectorOrigin();

	// Player starts stand where the default pawn's capsule fits on the floor socket.
	const AGameModeBase* GameMode = Cast<AGameModeBase>(GetOwner());
	const ACharacter* PawnDefaults = GameMode && GameMode->DefaultPawnClass ? Cast<ACharacter>(GameMode->DefaultPawnClass->GetDefaultObject()) : nullptr;
	const double StartLift = PawnDefaults ? PawnDefaults->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.0;

	auto Spawn = [this, World](UClass* Class, const FTransform& Transform, const FName Label)
	{
		AActor* Actor = World->SpawnActorDeferred<AActor>(Class, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Actor)
		{
			return;
		}
		if (AHollowSectorMarker* Marker = Cast<AHollowSectorMarker>(Actor))
		{
			Marker->SetLabel(Label);
		}
		Actor->FinishSpawning(Transform);
		SpawnedActors.Add(Actor);
	};

	// Sockets in manifest order with their room's distance from Spawn; the biome decides which rule spawns where.
	struct FSocketPlace
	{
		const FHollowRoomSocket* Socket = nullptr;
		FTransform RoomTransform;
	};
	const TArray<FIntPoint> Links = HollowSectorGraph::LinkPairs(Layout);
	const int32 SpawnRoom = HollowSectorAuthorityPrivate::FindRole(Layout, EHollowRoomRole::Spawn);
	TArray<FSocketPlace> Places;
	TArray<FHollowSocketSlot> Slots;
	for (int32 RoomIndex = 0; RoomIndex < Manifest.Rooms.Num(); ++RoomIndex)
	{
		const UHollowRoomDefinition* Room = Manifest.Rooms[RoomIndex].Room;
		const FTransform RoomTransform = HollowSectorManifest::RoomTransform(Layout.Rooms[RoomIndex], FIntPoint(Room->Footprint.X, Room->Footprint.Y), *Metrics, Origin);
		const int32 Distance = SpawnRoom != INDEX_NONE ? HollowSectorGraph::ShortestPath(Manifest.Rooms.Num(), Links, SpawnRoom, RoomIndex) : INDEX_NONE;
		for (const FHollowRoomSocket& Socket : Room->Sockets)
		{
			Places.Add({&Socket, RoomTransform});
			Slots.Add({Socket.SocketType, Distance});
		}
	}
	const TArray<int32> Assigned = Biome->AssignSocketRules(Slots);
	for (int32 Slot = 0; Slot < Places.Num(); ++Slot)
	{
		const FHollowSocketActorRule* Rule = Biome->SocketActors.IsValidIndex(Assigned[Slot]) ? &Biome->SocketActors[Assigned[Slot]] : nullptr;
		UClass* Class = Rule ? Rule->ActorClass.Get() : nullptr;
		if (!Class)
		{
			continue;
		}
		const FHollowRoomSocket& Socket = *Places[Slot].Socket;
		FTransform Transform = FTransform(Rule->SpawnOffset) * Socket.Transform * Places[Slot].RoomTransform;
		if (Class->IsChildOf<APlayerStart>())
		{
			Transform.AddToTranslation(FVector(0.0, 0.0, StartLift));
		}
		Spawn(Class, Transform, Socket.SocketType.GetTagName());
	}
	for (int32 Index = 0; Index < Layout.Connections.Num(); ++Index)
	{
		// Variants (bulkhead shapes) follow the seed, so a seed always looks the same for QA.
		const FHollowPlacedConnection& Connection = Layout.Connections[Index];
		if (UClass* Class = Biome->FindConnectionActor(Connection.Kind, HollowRandom::DeriveSeed(Manifest.Seed, Index)).Get())
		{
			Spawn(Class, HollowSectorManifest::OpeningTransform(Connection.CellA, Connection.SideA, *Metrics, Origin), FName(*HollowEnumToString(Connection.Kind)));
		}
	}
	UE_LOG(LogHollowGame, Log, TEXT("Sector build %d: %d gameplay actors spawned"), Manifest.BuildId, SpawnedActors.Num());
}

void UHollowSectorAuthorityComponent::BuildNavigation()
{
	UWorld* World = GetWorld();
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!Navigation)
	{
		UE_LOG(LogHollowGame, Warning, TEXT("No navigation system in %s: AI cannot path in this sector."), *World->GetMapName());
		bNavigationReady = true;
		return;
	}

	// One synchronous build behind the loading phase: the sector is small and its geometry exists only now (ADR-0004).
	const double Start = FPlatformTime::Seconds();
	Navigation->Build();
	const double Milliseconds = (FPlatformTime::Seconds() - Start) * 1000.0;

	const UHollowSectorAssemblySubsystem* Assembly = World->GetSubsystem<UHollowSectorAssemblySubsystem>();
	const FHollowSectorLayout& Layout = Assembly->GetLayout();
	using namespace HollowSectorAuthorityPrivate;
	const int32 SpawnRoom = FindRole(Layout, EHollowRoomRole::Spawn);
	const int32 ExtractionRoom = FindRole(Layout, EHollowRoomRole::Extraction);
	const TOptional<FVector> From = SpawnRoom != INDEX_NONE ? SocketLocation(Assembly->GetManifest(), Layout, SpawnRoom, HollowTags::Socket_PlayerSpawn) : TOptional<FVector>();
	const TOptional<FVector> To = ExtractionRoom != INDEX_NONE ? SocketLocation(Assembly->GetManifest(), Layout, ExtractionRoom, HollowTags::Socket_Extraction) : TOptional<FVector>();
	// With the Standard filter, the way players and ordinary enemies see the sector: shut bulkheads are walls (S3-01).
	const TSubclassOf<UNavigationQueryFilter> Filter = GetDefault<UHollowNavSettings>()->StandardFilter.LoadSynchronous();
	const UNavigationPath* Path = From && To ? UNavigationSystemV1::FindPathToLocationSynchronously(World, *From, *To, nullptr, Filter) : nullptr;
	const bool bPath = Path && Path->IsValid() && !Path->IsPartial();
	if (bPath)
	{
		UE_LOG(LogHollowGame, Log, TEXT("Sector build %d: navmesh built in %.1f ms, path Spawn -> Extraction ok (%d points)"), Assembly->GetBuildId(),
			Milliseconds, Path->PathPoints.Num());
	}
	else
	{
		UE_LOG(LogHollowGame, Error, TEXT("Sector build %d: navmesh built in %.1f ms but there is no full path Spawn -> Extraction."), Assembly->GetBuildId(), Milliseconds);
	}
	bNavigationReady = true;
}

void UHollowSectorAuthorityComponent::HandleReport(APlayerController* Player, const int32 BuildId, const uint32 LayoutHash)
{
	const FHollowSectorManifest& Manifest = GetSectorState()->GetManifest();
	if (!Player || BuildId != Manifest.BuildId)
	{
		UE_LOG(LogHollowGame, Verbose, TEXT("Stale sector report for build %d from %s ignored."), BuildId, *GetNameSafe(Player));
		return;
	}
	if (LayoutHash != Manifest.LayoutHash)
	{
		UE_LOG(LogHollowGame, Error, TEXT("Sector desync: %s built build %d as %08X, server has %08X."), *GetNameSafe(Player), BuildId, LayoutHash, Manifest.LayoutHash);
		const AGameModeBase* GameMode = Cast<AGameModeBase>(GetOwner());
		if (!Player->IsLocalController() && GameMode && GameMode->GameSession)
		{
			GameMode->GameSession->KickPlayer(Player, LOCTEXT("Desync", "Your sector does not match the host's (content mismatch)."));
		}
		return;
	}
	Reported.AddUnique(Player);
	UE_LOG(LogHollowGame, Log, TEXT("Sector build %d: %s ready (hash %08X matches)"), BuildId, *Player->GetName(), LayoutHash);
	TryFinish();
}

void UHollowSectorAuthorityComponent::HandlePlayerLeft(AController* Exiting)
{
	Reported.Remove(Cast<APlayerController>(Exiting));
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::TryFinish);   // the leaving controller is still listed now
}

void UHollowSectorAuthorityComponent::TryFinish()
{
	if (bSectorReady || !bServerAssembled || !bNavigationReady)
	{
		return;
	}
	int32 Players = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (!Reported.Contains(It->Get()))
		{
			return;
		}
		++Players;
	}

	bSectorReady = true;
	GetWorld()->GetTimerManager().ClearTimer(ReadyTimer);
	GetSectorState()->SetSectorReady(true);
	UE_LOG(LogHollowGame, Log, TEXT("Sector build %d ready: %d player(s), every hash matches"), GetSectorState()->GetManifest().BuildId, Players);

	AGameModeBase* GameMode = Cast<AGameModeBase>(GetOwner());
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* Player = It->Get(); Player && !Player->GetPawn() && GameMode)
		{
			// Seamless travel remembered a start spot before the sector existed (the world settings at the origin):
			// forget it so the game mode picks one of the PlayerSpawn starts.
			Player->StartSpot = nullptr;
			GameMode->RestartPlayer(Player);
		}
	}
}

void UHollowSectorAuthorityComponent::HandleReadyTimeout()
{
	const AGameModeBase* GameMode = Cast<AGameModeBase>(GetOwner());
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Player = It->Get();
		if (Player && !Reported.Contains(Player) && !Player->IsLocalController() && GameMode && GameMode->GameSession)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("%s did not assemble the sector in time; kicked."), *Player->GetName());
			GameMode->GameSession->KickPlayer(Player, LOCTEXT("Timeout", "Loading the sector took too long."));
		}
	}
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::TryFinish);
}

void UHollowSectorAuthorityComponent::ClearSector()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Player = It->Get();
		if (APawn* Pawn = Player ? Player->GetPawn() : nullptr)
		{
			Player->UnPossess();
			Pawn->Destroy();
		}
	}
	for (const TWeakObjectPtr<AActor>& Actor : SpawnedActors)
	{
		if (Actor.IsValid())
		{
			Actor->Destroy();
		}
	}
	SpawnedActors.Reset();
	if (UHollowSectorTopologySubsystem* Topology = UHollowSectorTopologySubsystem::Get(this))
	{
		Topology->SetTopology(FHollowSectorTopology());   // the AI forgets the old sector (the director recalls its stalker)
	}
	Reported.Reset();
	bServerAssembled = false;
	bNavigationReady = false;
	bSectorReady = false;
	if (UHollowSectorStateComponent* State = GetSectorState())
	{
		State->SetSectorReady(false);
	}
}

UHollowSectorStateComponent* UHollowSectorAuthorityComponent::GetSectorState() const
{
	return UHollowSectorStateComponent::Get(GetWorld());
}

FString UHollowSectorAuthorityComponent::Describe() const
{
	const UHollowSectorStateComponent* State = GetSectorState();
	const int32 BuildId = State ? State->GetManifest().BuildId : 0;
	int32 Players = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		++Players;
	}
	return FString::Printf(TEXT("authority: build %d, server assembled %d, navigation %d, reported %d/%d, ready %d, %d gameplay actors"), BuildId,
		bServerAssembled, bNavigationReady, Reported.Num(), Players, bSectorReady, SpawnedActors.Num());
}

#undef LOCTEXT_NAMESPACE
