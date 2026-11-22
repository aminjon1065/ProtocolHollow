#include "Threat/HollowThreatDirectorSubsystem.h"

#include "Enemies/HollowEnemyCharacter.h"
#include "Enemies/HollowEnemySpawning.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HollowAILog.h"
#include "Sector/HollowSectorTopologySubsystem.h"
#include "Threat/HollowActivityHeatmapSubsystem.h"
#include "Threat/HollowAIPlayers.h"
#include "Threat/HollowThreatConfig.h"
#include "Threat/HollowVentNetworkSubsystem.h"
#include "TimerManager.h"
#include "Util/HollowEnumUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowThreatDirectorSubsystem)

UHollowThreatDirectorSubsystem* UHollowThreatDirectorSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UHollowThreatDirectorSubsystem>() : nullptr;
}

bool UHollowThreatDirectorSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UHollowThreatDirectorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Config = UHollowThreatConfig::Get();
	// After the vents and the heatmap: they rebuild on the same topology change first.
	Collection.InitializeDependency<UHollowVentNetworkSubsystem>();
	Collection.InitializeDependency<UHollowActivityHeatmapSubsystem>();
	if (UHollowSectorTopologySubsystem* Topology = Collection.InitializeDependency<UHollowSectorTopologySubsystem>())
	{
		TopologyHandle = Topology->OnTopologyChanged.AddUObject(this, &ThisClass::HandleTopologyChanged);
	}
}

void UHollowThreatDirectorSubsystem::Deinitialize()
{
	if (UHollowSectorTopologySubsystem* Topology = UHollowSectorTopologySubsystem::Get(this))
	{
		Topology->OnTopologyChanged.Remove(TopologyHandle);
	}
	Super::Deinitialize();
}

EHollowThreatMode UHollowThreatDirectorSubsystem::GetModeFor(const AActor* Enemy) const
{
	return Enemy && Enemy == Stalker.Get() ? Mode : EHollowThreatMode::Stalk;
}

void UHollowThreatDirectorSubsystem::HandleTopologyChanged()
{
	UWorld* World = GetWorld();
	const UHollowSectorTopologySubsystem* Topology = UHollowSectorTopologySubsystem::Get(this);
	if (!World || World->GetNetMode() == NM_Client || !Config || !Topology)
	{
		return;
	}
	if (Topology->GetTopology().IsEmpty())
	{
		// The sector is gone: so is its stalker.
		World->GetTimerManager().ClearTimer(SpawnTimer);
		World->GetTimerManager().ClearTimer(ModeTimer);
		World->GetTimerManager().ClearTimer(HeatTimer);
		if (AHollowEnemyCharacter* Old = Stalker.Get())
		{
			Old->Destroy();
		}
		Stalker.Reset();
		return;
	}
	if (Stalker.IsValid() || Config->Director.StalkerClass.IsNull())
	{
		return;
	}
	UE_LOG(LogHollowAI, Log, TEXT("Director: the stalker comes in %.0f s"), Config->Director.SpawnDelaySeconds);
	World->GetTimerManager().SetTimer(SpawnTimer, this, &ThisClass::HandleSpawnTimer, Config->Director.SpawnDelaySeconds, /*bLoop=*/false);
}

void UHollowThreatDirectorSubsystem::HandleSpawnTimer()
{
	if (!Stalker.IsValid())
	{
		SpawnStalker();
	}
}

bool UHollowThreatDirectorSubsystem::FindSpawnLocation(FVector& OutLocation) const
{
	TArray<FVector> Players;
	HollowAIPlayers::ForEach(GetWorld(), [&Players](APawn& Pawn) { Players.Add(Pawn.GetActorLocation()); });
	const UHollowVentNetworkSubsystem* Vents = UHollowVentNetworkSubsystem::Get(this);
	if (const int32 Node = Vents ? Vents->FarthestNodeFrom(INDEX_NONE, Players) : INDEX_NONE; Node != INDEX_NONE)
	{
		OutLocation = Vents->GetNodeLocation(Node);
		return true;
	}
	// No vents: the room centre farthest from everyone.
	const UHollowSectorTopologySubsystem* Topology = UHollowSectorTopologySubsystem::Get(this);
	double BestDistance = -1.0;
	for (const FHollowTopologyRoom& Room : Topology ? Topology->GetTopology().Rooms : TArray<FHollowTopologyRoom>())
	{
		double Nearest = TNumericLimits<double>::Max();
		for (const FVector& Player : Players)
		{
			Nearest = FMath::Min(Nearest, FVector::Dist(Room.Bounds.GetCenter(), Player));
		}
		if (Nearest > BestDistance)
		{
			BestDistance = Nearest;
			OutLocation = FVector(Room.Bounds.GetCenter().X, Room.Bounds.GetCenter().Y, Room.Bounds.Min.Z);
		}
	}
	return BestDistance >= 0.0;
}

AHollowEnemyCharacter* UHollowThreatDirectorSubsystem::SpawnStalker()
{
	UWorld* World = GetWorld();
	FVector Location;
	if (!World || World->GetNetMode() == NM_Client || !Config || !FindSpawnLocation(Location))
	{
		return nullptr;
	}
	AHollowEnemyCharacter* Spawned = HollowEnemySpawning::SpawnOnNavmesh(World, Config->Director.StalkerClass.LoadSynchronous(), Location,
		Config->Director.SpawnSearchExtent, FRotator::ZeroRotator);
	if (Spawned)
	{
		UE_LOG(LogHollowAI, Log, TEXT("Director: %s comes out of the vents at %s"), *Spawned->GetName(), *Spawned->GetActorLocation().ToCompactString());
		Spawned->EmergeFromVent();
		Register(Spawned);
	}
	return Spawned;
}

void UHollowThreatDirectorSubsystem::Register(AHollowEnemyCharacter* Enemy)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpawnTimer);   // brought early (dev) or by hand (tests): no second one
	}
	Stalker = Enemy;
	SetMode(EHollowThreatMode::Stalk);
}

void UHollowThreatDirectorSubsystem::ReportStrike(const AActor* Enemy)
{
	if (Enemy && Enemy == Stalker.Get() && Mode == EHollowThreatMode::Hunt)
	{
		SetMode(EHollowThreatMode::Retreat);
	}
}

void UHollowThreatDirectorSubsystem::ReportStunned(const AActor* Enemy)
{
	if (Enemy && Enemy == Stalker.Get())
	{
		SetMode(EHollowThreatMode::Retreat);   // once it can move again, it is gone
	}
}

void UHollowThreatDirectorSubsystem::ReportRepelled(const AActor* Enemy)
{
	if (Enemy && Enemy == Stalker.Get() && Mode != EHollowThreatMode::Retreat)
	{
		UE_LOG(LogHollowAI, Log, TEXT("Director: %s flees the Aether light"), *GetNameSafe(Enemy));
		SetMode(EHollowThreatMode::Retreat);
	}
}

void UHollowThreatDirectorSubsystem::SetMode(const EHollowThreatMode NewMode)
{
	UWorld* World = GetWorld();
	if (!World || !Config)
	{
		return;
	}
	Mode = NewMode;
	FTimerManager& Timers = World->GetTimerManager();
	Timers.ClearTimer(HeatTimer);
	const FHollowDirectorTuning& Tuning = Config->Director;
	const float Seconds = NewMode == EHollowThreatMode::Stalk ? Tuning.StalkSeconds : (NewMode == EHollowThreatMode::Hunt ? Tuning.HuntSeconds : Tuning.RetreatSeconds);
	Timers.SetTimer(ModeTimer, this, &ThisClass::HandleModeTimer, Seconds, /*bLoop=*/false);
	if (NewMode == EHollowThreatMode::Stalk)
	{
		Timers.SetTimer(HeatTimer, this, &ThisClass::CheckHeat, Tuning.HeatCheckSeconds, /*bLoop=*/true);
	}
	UE_LOG(LogHollowAI, Log, TEXT("Director: %s %s for up to %.0f s"), *GetNameSafe(Stalker.Get()), *HollowEnumToString(NewMode), Seconds);
	OnModeChanged.Broadcast(Stalker.Get(), NewMode);
}

void UHollowThreatDirectorSubsystem::HandleModeTimer()
{
	switch (Mode)
	{
	case EHollowThreatMode::Stalk:   SetMode(EHollowThreatMode::Hunt); break;
	case EHollowThreatMode::Hunt:    SetMode(EHollowThreatMode::Retreat); break;
	case EHollowThreatMode::Retreat: SetMode(EHollowThreatMode::Stalk); break;
	}
}

void UHollowThreatDirectorSubsystem::CheckHeat()
{
	const UHollowActivityHeatmapSubsystem* Heatmap = UHollowActivityHeatmapSubsystem::Get(this);
	const int32 Room = Heatmap ? Heatmap->HottestRoom(/*bWithoutPlayers=*/true) : INDEX_NONE;
	if (Mode == EHollowThreatMode::Stalk && Stalker.IsValid() && Room != INDEX_NONE && Heatmap->GetRoomHeat(Room) >= Config->Director.HuntHeatThreshold)
	{
		UE_LOG(LogHollowAI, Log, TEXT("Director: room %d is hot (%.1f)"), Room, Heatmap->GetRoomHeat(Room));
		SetMode(EHollowThreatMode::Hunt);
	}
}

FString UHollowThreatDirectorSubsystem::Describe() const
{
	const UWorld* World = GetWorld();
	const float Left = World ? World->GetTimerManager().GetTimerRemaining(ModeTimer) : 0.f;
	const float SpawnIn = World ? World->GetTimerManager().GetTimerRemaining(SpawnTimer) : 0.f;
	return FString::Printf(TEXT("stalker %s, %s, %.1f s left%s"), *GetNameSafe(Stalker.Get()), *HollowEnumToString(Mode), FMath::Max(Left, 0.f),
		SpawnIn > 0.f ? *FString::Printf(TEXT(", spawns in %.1f s"), SpawnIn) : TEXT(""));
}
