#include "Brain/HollowPathFollowingComponent.h"

#include "Brain/HollowAIController.h"
#include "Components/ActorComponent.h"
#include "Interfaces/HollowStructural.h"
#include "NavLinkCustomInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowPathFollowingComponent)

void UHollowPathFollowingComponent::StartUsingCustomLink(INavLinkCustomInterface* CustomNavLink, const FVector& DestPoint)
{
	Super::StartUsingCustomLink(CustomNavLink, DestPoint);
	UObject* LinkObject = CustomNavLink ? CustomNavLink->GetLinkOwner() : nullptr;   // the link component of a door or bulkhead
	const UActorComponent* LinkComponent = Cast<UActorComponent>(LinkObject);
	AActor* LinkActor = LinkComponent ? LinkComponent->GetOwner() : Cast<AActor>(LinkObject);
	const IHollowStructural* Structural = Cast<IHollowStructural>(LinkActor);
	if (Structural && Structural->GetPassage() != EHollowPassage::Clear)
	{
		if (AHollowAIController* Controller = Cast<AHollowAIController>(GetOwner()))
		{
			Controller->ReportObstacle(LinkActor);
		}
	}
}
