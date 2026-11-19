#pragma once

#include "GameFramework/Actor.h"
#include "Interfaces/HollowToolTarget.h"

#include "HollowToolTestTarget.generated.h"

class UStaticMeshComponent;

/**
 * Development target for continuous tools (S2-03): accepts any tool work and logs every sample with server time, so
 * sample rate, gaps and totals can be checked in automation and multi-process tests. Real targets: doors (S2-04).
 */
UCLASS()
class HOLLOWGAME_API AHollowToolTestTarget : public AActor, public IHollowToolTarget
{
	GENERATED_BODY()

public:
	explicit AHollowToolTestTarget(const FObjectInitializer& ObjectInitializer);

	// IHollowToolTarget
	virtual bool CanAcceptToolWork(const FHollowToolWork& Work) const override;
	virtual void ApplyToolWork(const FHollowToolWork& Work) override;

	float GetTotalWork(const FGameplayTag& ToolTag) const;
	int32 GetSamples(const FGameplayTag& ToolTag) const;

private:
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Tool")
	TObjectPtr<UStaticMeshComponent> Mesh;

	TMap<FGameplayTag, float> WorkByTool;
	TMap<FGameplayTag, int32> SamplesByTool;
};
