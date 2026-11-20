#include "EQS/HollowEnvQueryNodes.h"

#include "Brain/HollowStalkerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/HollowLightSource.h"
#include "Sector/HollowSectorTopologySubsystem.h"
#include "Threat/HollowActivityHeatmapSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowEnvQueryNodes)

#define LOCTEXT_NAMESPACE "HollowEnvQueryNodes"

namespace HollowEnvQueryPrivate
{
	/** The querier's focus room in the topology, or null. */
	const FHollowTopologyRoom* FocusRoom(const FEnvQueryInstance& QueryInstance, int32* OutIndex = nullptr)
	{
		const APawn* Querier = Cast<APawn>(QueryInstance.Owner.Get());
		const AHollowStalkerController* Controller = Querier ? Cast<AHollowStalkerController>(Querier->GetController()) : nullptr;
		const UHollowSectorTopologySubsystem* Topology = UHollowSectorTopologySubsystem::Get(Querier);
		const int32 Index = Controller ? Controller->GetFocusRoom() : INDEX_NONE;
		if (OutIndex)
		{
			*OutIndex = Index;
		}
		return Topology && Topology->GetTopology().Rooms.IsValidIndex(Index) ? &Topology->GetTopology().Rooms[Index] : nullptr;
	}
}

UHollowEnvQueryGenerator_FocusRoomPoints::UHollowEnvQueryGenerator_FocusRoomPoints(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ItemType = UEnvQueryItemType_Point::StaticClass();
}

void UHollowEnvQueryGenerator_FocusRoomPoints::GenerateItems(FEnvQueryInstance& QueryInstance) const
{
	const FHollowTopologyRoom* Room = HollowEnvQueryPrivate::FocusRoom(QueryInstance);
	if (!Room || PointSpacing <= 0.f)
	{
		return;
	}
	const FBox& Bounds = Room->Bounds;
	TArray<FNavLocation> Points;
	for (double X = Bounds.Min.X + WallMargin; X <= Bounds.Max.X - WallMargin; X += PointSpacing)
	{
		for (double Y = Bounds.Min.Y + WallMargin; Y <= Bounds.Max.Y - WallMargin; Y += PointSpacing)
		{
			Points.Add(FNavLocation(FVector(X, Y, Bounds.Min.Z)));   // on the floor; the projection finds the navmesh
		}
	}
	ProjectAndFilterNavPoints(Points, QueryInstance);
	StoreNavPoints(Points, QueryInstance);
}

UHollowEnvQueryTest_OutOfLightCone::UHollowEnvQueryTest_OutOfLightCone(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Cost = EEnvTestCost::High;   // a trace per lamp and point
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
	SetWorkOnFloatValues(false);
	TestPurpose = EEnvTestPurpose::Filter;
}

void UHollowEnvQueryTest_OutOfLightCone::RunTest(FEnvQueryInstance& QueryInstance) const
{
	const UObject* Owner = QueryInstance.Owner.Get();
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}
	BoolValue.BindData(Owner, QueryInstance.QueryID);
	const bool bWanted = BoolValue.GetValue();
	TArray<TPair<FHollowLightCone, const AActor*>> Cones;
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		FHollowLightCone Cone;
		if (const IHollowLightSource* Light = Cast<IHollowLightSource>(*It); Light && Light->GetLightCone(Cone))
		{
			Cones.Emplace(Cone, *It);
		}
	}
	for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
	{
		const FVector Body = GetItemLocation(QueryInstance, It.GetIndex()) + FVector(0.0, 0.0, PointHeight);
		bool bLit = false;
		for (const TPair<FHollowLightCone, const AActor*>& Cone : Cones)
		{
			if (Cone.Key.Contains(Body))
			{
				FCollisionQueryParams Params(SCENE_QUERY_STAT(HollowLightCone), /*bTraceComplex=*/false, Cone.Value);
				Params.AddIgnoredActor(Cast<AActor>(Owner));
				bLit = !World->LineTraceTestByChannel(Cone.Key.Origin, Body, ECC_Visibility, Params);
				if (bLit)
				{
					break;
				}
			}
		}
		It.SetScore(TestPurpose, FilterType, !bLit, bWanted);
	}
}

FText UHollowEnvQueryTest_OutOfLightCone::GetDescriptionTitle() const
{
	return LOCTEXT("Title", "Out of every lamp");
}

void UHollowEnvQueryContext_FocusRoomHotPortal::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	int32 RoomIndex = INDEX_NONE;
	const FHollowTopologyRoom* Room = HollowEnvQueryPrivate::FocusRoom(QueryInstance, &RoomIndex);
	if (!Room)
	{
		return;
	}
	const UObject* Owner = QueryInstance.Owner.Get();
	const UHollowActivityHeatmapSubsystem* Heatmap = UHollowActivityHeatmapSubsystem::Get(Owner);
	const UHollowSectorTopologySubsystem* Topology = UHollowSectorTopologySubsystem::Get(Owner);
	const int32 Portal = Heatmap ? Heatmap->HottestPortalOf(RoomIndex) : INDEX_NONE;
	const FVector Where = Topology && Topology->GetTopology().Portals.IsValidIndex(Portal) ? Topology->GetTopology().Portals[Portal].Location
		: FVector(Room->Bounds.GetCenter().X, Room->Bounds.GetCenter().Y, Room->Bounds.Min.Z);
	UEnvQueryItemType_Point::SetContextHelper(ContextData, Where);
}

#undef LOCTEXT_NAMESPACE
