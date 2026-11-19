#include "World/HollowToolTestTarget.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "HollowGameLog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowToolTestTarget)

AHollowToolTestTarget::AHollowToolTestTarget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
}

bool AHollowToolTestTarget::CanAcceptToolWork(const FHollowToolWork& Work) const
{
	return Work.ToolTag.IsValid();
}

void AHollowToolTestTarget::ApplyToolWork(const FHollowToolWork& Work)
{
	if (!HasAuthority())
	{
		return;
	}
	float& Total = WorkByTool.FindOrAdd(Work.ToolTag);
	int32& Samples = SamplesByTool.FindOrAdd(Work.ToolTag);
	Total += Work.Amount;
	++Samples;
	UE_LOG(LogHollowGame, Log, TEXT("ToolTarget %s: %s +%.3f over %.3f s at t=%.3f (total %.2f, %d samples)"), *GetName(),
		*Work.ToolTag.ToString(), Work.Amount, Work.DeltaSeconds, GetWorld()->GetTimeSeconds(), Total, Samples);
}

float AHollowToolTestTarget::GetTotalWork(const FGameplayTag& ToolTag) const
{
	const float* Total = WorkByTool.Find(ToolTag);
	return Total ? *Total : 0.f;
}

int32 AHollowToolTestTarget::GetSamples(const FGameplayTag& ToolTag) const
{
	const int32* Samples = SamplesByTool.Find(ToolTag);
	return Samples ? *Samples : 0;
}
