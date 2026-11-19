#pragma once

#include "GameplayTagContainer.h"
#include "UObject/Interface.h"

#include "HollowToolTarget.generated.h"

class AActor;

/** One server sample of a continuous tool (welder, cutter) on a target (Docs/Architecture/03 sec. 3.2). */
USTRUCT(BlueprintType)
struct FHollowToolWork
{
	GENERATED_BODY()

	/** Hollow.Ability.Tool.*: the target decides what the work means (weld, cut). */
	UPROPERTY(BlueprintReadOnly, Category = "Tool")
	FGameplayTag ToolTag;

	UPROPERTY(BlueprintReadOnly, Category = "Tool")
	TWeakObjectPtr<AActor> Instigator;

	/** Work units: tool rate x server seconds covered by this sample (never client time). */
	UPROPERTY(BlueprintReadOnly, Category = "Tool")
	float Amount = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Tool")
	float DeltaSeconds = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Tool")
	FVector ImpactPoint = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Tool")
	FVector ImpactNormal = FVector::ZeroVector;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UHollowToolTarget : public UInterface
{
	GENERATED_BODY()
};

/**
 * Something a continuous tool can work on: doors and seams (Physics), test targets. World objects keep their
 * integrity as replicated component state, changed only here, on the server (CLAUDE.md sec. 3, Q4).
 */
class HOLLOWCORE_API IHollowToolTarget
{
	GENERATED_BODY()

public:
	virtual bool CanAcceptToolWork(const FHollowToolWork& Work) const = 0;

	/** Server only. */
	virtual void ApplyToolWork(const FHollowToolWork& Work) = 0;
};
