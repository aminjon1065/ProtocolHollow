#pragma once

#include "NavigationData.h"
#include "NavLinkCustomInterface.h"
#include "Subsystems/WorldSubsystem.h"

#include "HollowNavUpdateSubsystem.generated.h"

class UNavArea;

/**
 * Navigation change hub of a world (ADR-0004): doors report link area changes here. Changing a link's area does not
 * rebuild tiles, so the engine does not invalidate paths through it; this subsystem does, for every path that AI
 * registered with WatchPath. Invalidated paths re-path themselves with their original filter.
 */
UCLASS()
class HOLLOWPHYSICS_API UHollowNavUpdateSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_FourParams(FOnHollowDoorNavChanged, const AActor* /*Door*/, FNavLinkId /*LinkId*/, TSubclassOf<UNavArea> /*Old*/, TSubclassOf<UNavArea> /*New*/);

	/** Keeps an eye on a path (weakly) so it re-paths when a door on it changes. AI calls this for each new path (S3). */
	void WatchPath(const FNavPathSharedPtr& Path);

	/** Server: a door's link changed area. Invalidates watched paths through the link and tells listeners. */
	void NotifyLinkChanged(const AActor* Door, FNavLinkId LinkId, TSubclassOf<UNavArea> OldArea, TSubclassOf<UNavArea> NewArea);

	static bool PathUsesLink(const FNavigationPath& Path, FNavLinkId LinkId);

	FOnHollowDoorNavChanged OnDoorNavChanged;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	TArray<FNavPathWeakPtr> WatchedPaths;
};
