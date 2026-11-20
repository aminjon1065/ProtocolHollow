#include "Threat/HollowVentNetworkSubsystem.h"

#include "Engine/World.h"
#include "HollowAILog.h"
#include "Sector/HollowSectorTopologySubsystem.h"
#include "Threat/HollowThreatConfig.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowVentNetworkSubsystem)

UHollowVentNetworkSubsystem* UHollowVentNetworkSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UHollowVentNetworkSubsystem>() : nullptr;
}

bool UHollowVentNetworkSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UHollowVentNetworkSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Config = UHollowThreatConfig::Get();
	if (UHollowSectorTopologySubsystem* Topology = Collection.InitializeDependency<UHollowSectorTopologySubsystem>())
	{
		TopologyHandle = Topology->OnTopologyChanged.AddUObject(this, &ThisClass::Rebuild);
	}
}

void UHollowVentNetworkSubsystem::Deinitialize()
{
	if (UHollowSectorTopologySubsystem* Topology = UHollowSectorTopologySubsystem::Get(this))
	{
		Topology->OnTopologyChanged.Remove(TopologyHandle);
	}
	Super::Deinitialize();
}

void UHollowVentNetworkSubsystem::Rebuild()
{
	Nodes.Reset();
	Links.Reset();
	const UHollowSectorTopologySubsystem* Topology = UHollowSectorTopologySubsystem::Get(this);
	if (!Topology || !Config)
	{
		return;
	}
	const FHollowSectorTopology& Sector = Topology->GetTopology();
	for (const FHollowTopologyVent& Vent : Sector.Vents)
	{
		Nodes.Add(Vent.Location);
	}
	for (int32 A = 0; A < Sector.Vents.Num(); ++A)
	{
		for (int32 B = A + 1; B < Sector.Vents.Num(); ++B)
		{
			const int32 Rooms = Sector.RoomDistance(Sector.Vents[A].Room, Sector.Vents[B].Room);
			if (Rooms != INDEX_NONE && Rooms <= Config->Vents.MaxRoomLinks)
			{
				Links.Add({A, B, FMath::Max(Rooms, 1) * Config->Vents.SecondsPerLink});
			}
		}
	}
	UE_LOG(LogHollowAI, Log, TEXT("Vent network: %d openings, %d links"), Nodes.Num(), Links.Num());
}

FVector UHollowVentNetworkSubsystem::GetNodeLocation(const int32 Node) const
{
	return Nodes.IsValidIndex(Node) ? Nodes[Node] : FVector::ZeroVector;
}

int32 UHollowVentNetworkSubsystem::NearestNode(const FVector& Location) const
{
	int32 Best = INDEX_NONE;
	for (int32 Node = 0; Node < Nodes.Num(); ++Node)
	{
		if (Best == INDEX_NONE || FVector::DistSquared(Nodes[Node], Location) < FVector::DistSquared(Nodes[Best], Location))
		{
			Best = Node;
		}
	}
	return Best;
}

float UHollowVentNetworkSubsystem::TravelSeconds(const int32 From, const int32 To) const
{
	if (!Nodes.IsValidIndex(From) || !Nodes.IsValidIndex(To))
	{
		return -1.f;
	}
	// A handful of nodes: plain Dijkstra over arrays.
	TArray<float> Cost;
	Cost.Init(-1.f, Nodes.Num());
	TArray<bool> Done;
	Done.Init(false, Nodes.Num());
	Cost[From] = 0.f;
	for (;;)
	{
		int32 Current = INDEX_NONE;
		for (int32 Node = 0; Node < Nodes.Num(); ++Node)
		{
			if (!Done[Node] && Cost[Node] >= 0.f && (Current == INDEX_NONE || Cost[Node] < Cost[Current]))
			{
				Current = Node;
			}
		}
		if (Current == INDEX_NONE || Current == To)
		{
			return Current == To ? Cost[To] : -1.f;
		}
		Done[Current] = true;
		for (const FHollowVentLink& Link : Links)
		{
			const int32 Other = Link.A == Current ? Link.B : (Link.B == Current ? Link.A : INDEX_NONE);
			if (Other != INDEX_NONE && !Done[Other] && (Cost[Other] < 0.f || Cost[Current] + Link.Seconds < Cost[Other]))
			{
				Cost[Other] = Cost[Current] + Link.Seconds;
			}
		}
	}
}

int32 UHollowVentNetworkSubsystem::FarthestNodeFrom(const int32 From, const TConstArrayView<FVector> Locations) const
{
	int32 Best = INDEX_NONE;
	double BestDistance = -1.0;
	for (int32 Node = 0; Node < Nodes.Num(); ++Node)
	{
		if (From != INDEX_NONE && Node != From && TravelSeconds(From, Node) < 0.f)
		{
			continue;
		}
		double Nearest = TNumericLimits<double>::Max();
		for (const FVector& Location : Locations)
		{
			Nearest = FMath::Min(Nearest, FVector::Dist(Nodes[Node], Location));
		}
		if (Nearest > BestDistance)
		{
			Best = Node;
			BestDistance = Nearest;
		}
	}
	return Best;
}

FString UHollowVentNetworkSubsystem::Describe() const
{
	FString Text = FString::Printf(TEXT("%d openings:"), Nodes.Num());
	for (int32 Node = 0; Node < Nodes.Num(); ++Node)
	{
		Text += FString::Printf(TEXT(" [%d] %s"), Node, *Nodes[Node].ToCompactString());
	}
	Text += FString::Printf(TEXT(" | %d links:"), Links.Num());
	for (const FHollowVentLink& Link : Links)
	{
		Text += FString::Printf(TEXT(" %d-%d %.1fs"), Link.A, Link.B, Link.Seconds);
	}
	return Text;
}
