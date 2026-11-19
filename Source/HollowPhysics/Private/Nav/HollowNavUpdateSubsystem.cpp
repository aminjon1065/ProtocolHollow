#include "Nav/HollowNavUpdateSubsystem.h"

#include "Algo/AnyOf.h"
#include "HollowPhysicsLog.h"
#include "NavAreas/NavArea.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowNavUpdateSubsystem)

bool UHollowNavUpdateSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UHollowNavUpdateSubsystem::WatchPath(const FNavPathSharedPtr& Path)
{
	if (Path.IsValid())
	{
		WatchedPaths.RemoveAll([](const FNavPathWeakPtr& Watched) { return !Watched.IsValid(); });
		WatchedPaths.AddUnique(Path);
	}
}

void UHollowNavUpdateSubsystem::NotifyLinkChanged(const AActor* Door, const FNavLinkId LinkId, const TSubclassOf<UNavArea> OldArea, const TSubclassOf<UNavArea> NewArea)
{
	int32 Invalidated = 0;
	for (const FNavPathWeakPtr& Watched : WatchedPaths)
	{
		if (const FNavPathSharedPtr Path = Watched.Pin(); Path.IsValid() && Path->IsValid() && PathUsesLink(*Path, LinkId))
		{
			Path->Invalidate();   // re-paths with its own query and filter (auto update on invalidation)
			++Invalidated;
		}
	}
	UE_LOG(LogHollowPhysics, Log, TEXT("%s: nav link %s -> %s, %d watched path(s) invalidated"), *GetNameSafe(Door), *GetNameSafe(OldArea.Get()),
		*GetNameSafe(NewArea.Get()), Invalidated);
	OnDoorNavChanged.Broadcast(Door, LinkId, OldArea, NewArea);
}

bool UHollowNavUpdateSubsystem::PathUsesLink(const FNavigationPath& Path, const FNavLinkId LinkId)
{
	return LinkId != FNavLinkId::Invalid && Algo::AnyOf(Path.GetPathPoints(), [LinkId](const FNavPathPoint& Point) { return Point.CustomNavLinkId == LinkId; });
}
