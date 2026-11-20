#pragma once

#include "Navigation/PathFollowingComponent.h"

#include "HollowPathFollowingComponent.generated.h"

/**
 * Path following of Hollow enemies (ADR-0017, Docs/Architecture/03 sec. 3.7): when a path segment starts on the nav
 * link of a door or bulkhead that does not let it through (IHollowStructural::GetPassage), the controller is told,
 * stops there and lets the brain open, wait for or break the obstacle before it moves on.
 */
UCLASS()
class HOLLOWAI_API UHollowPathFollowingComponent : public UPathFollowingComponent
{
	GENERATED_BODY()

protected:
	virtual void StartUsingCustomLink(INavLinkCustomInterface* CustomNavLink, const FVector& DestPoint) override;
};
