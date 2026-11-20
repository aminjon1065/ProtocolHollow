#include "Threat/HollowActivityHeatmapSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HollowAILog.h"
#include "Noise/HollowNoiseSubsystem.h"
#include "Sector/HollowSectorTopologySubsystem.h"
#include "Threat/HollowAIPlayers.h"
#include "Threat/HollowThreatConfig.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowActivityHeatmapSubsystem)

UHollowActivityHeatmapSubsystem* UHollowActivityHeatmapSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UHollowActivityHeatmapSubsystem>() : nullptr;
}

bool UHollowActivityHeatmapSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UHollowActivityHeatmapSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Config = UHollowThreatConfig::Get();
	if (UHollowSectorTopologySubsystem* Topology = Collection.InitializeDependency<UHollowSectorTopologySubsystem>())
	{
		TopologyHandle = Topology->OnTopologyChanged.AddUObject(this, &ThisClass::HandleTopologyChanged);
	}
}

void UHollowActivityHeatmapSubsystem::Deinitialize()
{
	if (UHollowSectorTopologySubsystem* Topology = UHollowSectorTopologySubsystem::Get(this))
	{
		Topology->OnTopologyChanged.Remove(TopologyHandle);
	}
	if (UHollowNoiseSubsystem* Noise = UHollowNoiseSubsystem::Get(this))
	{
		Noise->OnNoiseReported.Remove(NoiseHandle);
	}
	Super::Deinitialize();
}

void UHollowActivityHeatmapSubsystem::HandleTopologyChanged()
{
	const UHollowSectorTopologySubsystem* Topology = UHollowSectorTopologySubsystem::Get(this);
	UWorld* World = GetWorld();
	if (!Topology || !World || World->GetNetMode() == NM_Client || !Config)
	{
		return;
	}
	RoomHeat.Init(0.f, Topology->GetTopology().Rooms.Num());
	PortalHeat.Init(0.f, Topology->GetTopology().Portals.Num());
	LastRooms.Reset();
	if (Topology->GetTopology().IsEmpty())
	{
		World->GetTimerManager().ClearTimer(SampleTimer);
		return;
	}
	if (UHollowNoiseSubsystem* Noise = UHollowNoiseSubsystem::Get(this); Noise && !NoiseHandle.IsValid())
	{
		NoiseHandle = Noise->OnNoiseReported.AddUObject(this, &ThisClass::HandleNoise);
	}
	World->GetTimerManager().SetTimer(SampleTimer, this, &ThisClass::HandleSampleTimer, Config->Heatmap.SampleSeconds, /*bLoop=*/true);
}

void UHollowActivityHeatmapSubsystem::HandleSampleTimer()
{
	Sample(Config ? Config->Heatmap.SampleSeconds : 0.f);
}

TArray<int32> UHollowActivityHeatmapSubsystem::PlayerRooms() const
{
	TArray<int32> Rooms;
	const UHollowSectorTopologySubsystem* Topology = UHollowSectorTopologySubsystem::Get(this);
	if (Topology)
	{
		HollowAIPlayers::ForEach(GetWorld(), [&Rooms, Topology](APawn& Pawn) { Rooms.Add(Topology->GetTopology().RoomAt(Pawn.GetActorLocation())); });
	}
	return Rooms;
}

void UHollowActivityHeatmapSubsystem::Sample(const float DeltaSeconds)
{
	const UHollowSectorTopologySubsystem* Topology = UHollowSectorTopologySubsystem::Get(this);
	if (!Topology || !Config || RoomHeat.IsEmpty())
	{
		return;
	}
	const FHollowHeatmapTuning& Tuning = Config->Heatmap;
	const float Decay = FMath::Exp(-UE_LN2 * DeltaSeconds / FMath::Max(Tuning.HalfLifeSeconds, UE_KINDA_SMALL_NUMBER));
	for (float& Heat : RoomHeat)
	{
		Heat *= Decay;
	}
	for (float& Heat : PortalHeat)
	{
		Heat *= Decay;
	}

	const FHollowSectorTopology& Sector = Topology->GetTopology();
	TArray<TPair<TWeakObjectPtr<APawn>, int32>> Rooms;
	HollowAIPlayers::ForEach(GetWorld(), [&](APawn& Pawn)
	{
		const int32 Room = Sector.RoomAt(Pawn.GetActorLocation());
		Rooms.Emplace(&Pawn, Room);
		if (!RoomHeat.IsValidIndex(Room))
		{
			return;
		}
		RoomHeat[Room] += Tuning.PresencePerSecond * DeltaSeconds;
		const TPair<TWeakObjectPtr<APawn>, int32>* Last = LastRooms.FindByPredicate([&Pawn](const TPair<TWeakObjectPtr<APawn>, int32>& Entry) { return Entry.Key.Get() == &Pawn; });
		const int32 Portal = Last && Last->Value != Room ? Sector.PortalBetween(Last->Value, Room) : INDEX_NONE;
		if (PortalHeat.IsValidIndex(Portal))
		{
			PortalHeat[Portal] += Tuning.CrossingHeat;
		}
	});
	LastRooms = MoveTemp(Rooms);
}

void UHollowActivityHeatmapSubsystem::HandleNoise(const FHollowNoiseReport& Report)
{
	const UHollowSectorTopologySubsystem* Topology = UHollowSectorTopologySubsystem::Get(this);
	if (!Topology || !Config || !HollowAIPlayers::IsLivingPlayer(Report.Event.Instigator.Get()))
	{
		return;   // the enemies' own blows are not the squad's activity
	}
	const int32 Room = Topology->GetTopology().RoomAt(Report.Event.Location);
	if (RoomHeat.IsValidIndex(Room))
	{
		RoomHeat[Room] += Config->Heatmap.NoisePerLoudness * Report.Loudness;
	}
}

void UHollowActivityHeatmapSubsystem::AddRoomHeat(const int32 Room, const float Heat)
{
	if (RoomHeat.IsValidIndex(Room))
	{
		RoomHeat[Room] += Heat;
	}
}

void UHollowActivityHeatmapSubsystem::AddPortalHeat(const int32 Portal, const float Heat)
{
	if (PortalHeat.IsValidIndex(Portal))
	{
		PortalHeat[Portal] += Heat;
	}
}

float UHollowActivityHeatmapSubsystem::GetRoomHeat(const int32 Room) const
{
	return RoomHeat.IsValidIndex(Room) ? RoomHeat[Room] : 0.f;
}

float UHollowActivityHeatmapSubsystem::GetPortalHeat(const int32 Portal) const
{
	return PortalHeat.IsValidIndex(Portal) ? PortalHeat[Portal] : 0.f;
}

bool UHollowActivityHeatmapSubsystem::IsOccupied(const int32 Room) const
{
	return PlayerRooms().Contains(Room);
}

int32 UHollowActivityHeatmapSubsystem::HottestRoom(const bool bWithoutPlayers) const
{
	const TArray<int32> Occupied = bWithoutPlayers ? PlayerRooms() : TArray<int32>();
	int32 Best = INDEX_NONE;
	for (int32 Room = 0; Room < RoomHeat.Num(); ++Room)
	{
		if (RoomHeat[Room] > 0.f && !Occupied.Contains(Room) && (Best == INDEX_NONE || RoomHeat[Room] > RoomHeat[Best]))
		{
			Best = Room;
		}
	}
	return Best;
}

int32 UHollowActivityHeatmapSubsystem::HottestPortalOf(const int32 Room) const
{
	const UHollowSectorTopologySubsystem* Topology = UHollowSectorTopologySubsystem::Get(this);
	if (!Topology)
	{
		return INDEX_NONE;
	}
	const TArray<FHollowTopologyPortal>& Portals = Topology->GetTopology().Portals;
	int32 Best = INDEX_NONE;
	for (int32 Portal = 0; Portal < Portals.Num(); ++Portal)
	{
		const bool bOfRoom = Portals[Portal].RoomA == Room || Portals[Portal].RoomB == Room;
		if (bOfRoom && GetPortalHeat(Portal) > 0.f && (Best == INDEX_NONE || GetPortalHeat(Portal) > GetPortalHeat(Best)))
		{
			Best = Portal;
		}
	}
	return Best;
}

int32 UHollowActivityHeatmapSubsystem::SquadRoom() const
{
	const TArray<int32> Rooms = PlayerRooms();
	int32 Best = INDEX_NONE;
	int32 BestCount = 0;
	for (const int32 Room : Rooms)
	{
		const int32 Count = Rooms.FilterByPredicate([Room](const int32 Other) { return Other == Room; }).Num();
		if (Room != INDEX_NONE && (Count > BestCount || (Count == BestCount && Room < Best)))
		{
			Best = Room;
			BestCount = Count;
		}
	}
	return Best;
}

FString UHollowActivityHeatmapSubsystem::Describe() const
{
	TArray<int32> Order;
	for (int32 Room = 0; Room < RoomHeat.Num(); ++Room)
	{
		Order.Add(Room);
	}
	Order.StableSort([this](const int32 A, const int32 B) { return RoomHeat[A] > RoomHeat[B]; });
	FString Text = FString::Printf(TEXT("squad in room %d, hottest free room %d:"), SquadRoom(), HottestRoom(/*bWithoutPlayers=*/true));
	for (const int32 Room : Order)
	{
		if (RoomHeat[Room] > 0.f)
		{
			Text += FString::Printf(TEXT(" [%d] %.1f"), Room, RoomHeat[Room]);
		}
	}
	return Text;
}
