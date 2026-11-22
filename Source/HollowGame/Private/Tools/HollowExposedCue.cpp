#include "Tools/HollowExposedCue.h"

#include "Components/PointLightComponent.h"
#include "GameFramework/Actor.h"
#include "HollowGameLog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowExposedCue)

AHollowExposedCue::AHollowExposedCue(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bAutoDestroyOnRemove = true;

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetCastShadows(false);
	Glow->SetVisibility(false);
	SetRootComponent(Glow);
}

bool AHollowExposedCue::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	if (!MyTarget || !MyTarget->GetRootComponent())
	{
		return false;
	}
	AttachToComponent(MyTarget->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	SetActorRelativeLocation(GlowOffset);
	Glow->SetVisibility(true);
	UE_LOG(LogHollowGame, Log, TEXT("Exposed glow on %s (%s)"), *MyTarget->GetName(), MyTarget->HasAuthority() ? TEXT("server") : TEXT("replicated"));
	return true;
}

bool AHollowExposedCue::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	Glow->SetVisibility(false);
	UE_LOG(LogHollowGame, Log, TEXT("Exposed glow off %s"), *GetNameSafe(MyTarget));
	return Super::OnRemove_Implementation(MyTarget, Parameters);
}
