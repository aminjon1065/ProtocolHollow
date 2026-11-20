#include "Cues/HollowEnemyWindupCue.h"

#include "Audio/HollowAudioSubsystem.h"
#include "Components/PointLightComponent.h"
#include "Enemies/HollowEnemyCharacter.h"
#include "Enemies/HollowEnemyDefinition.h"

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

bool AHollowEnemyWindupCue::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	const AHollowEnemyCharacter* Enemy = Cast<AHollowEnemyCharacter>(MyTarget);
	const UHollowEnemyDefinition* Definition = Enemy ? Enemy->GetDefinition() : nullptr;
	UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(this);
	if (Audio && Definition && Definition->WindupSound.IsValid())
	{
		Audio->PlayAttached(Definition->WindupSound, Enemy->GetRootComponent());
	}
	return Super::OnActive_Implementation(MyTarget, Parameters);
}
