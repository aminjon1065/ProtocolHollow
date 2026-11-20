#include "Cues/HollowEnemyWindupCue.h"

#include "Components/PointLightComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowEnemyWindupCue)

AHollowEnemyWindupCue::AHollowEnemyWindupCue(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	SetRootComponent(Glow);
	bAutoAttachToOwner = true;     // rides on the attacker while it winds up
	bAutoDestroyOnRemove = true;   // gone when the blow lands
}
