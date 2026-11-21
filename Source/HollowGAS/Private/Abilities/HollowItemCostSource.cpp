#include "Abilities/HollowItemCostSource.h"

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowItemCostSource)

IHollowItemCostSource* IHollowItemCostSource::Find(const AActor* Owner)
{
	if (!Owner)
	{
		return nullptr;
	}
	if (IHollowItemCostSource* Source = Cast<IHollowItemCostSource>(const_cast<AActor*>(Owner)))
	{
		return Source;
	}
	return Cast<IHollowItemCostSource>(Owner->FindComponentByInterface(UHollowItemCostSource::StaticClass()));
}
