#pragma once

#include "GameplayTagContainer.h"
#include "UObject/Interface.h"

#include "HollowTargetStatusSource.generated.h"

/** What the visor shows about a tool target (local, cosmetic, from replicated state). */
USTRUCT(BlueprintType)
struct FHollowTargetStatus
{
	GENERATED_BODY()

	/** Fill of the status bar, 0..1 (a door: weld integrity). */
	UPROPERTY(BlueprintReadOnly, Category = "Status")
	float Value = 0.f;

	/** A threshold worth marking on the bar, 0..1, or negative for none (a door: the seal threshold). */
	UPROPERTY(BlueprintReadOnly, Category = "Status")
	float Marker = -1.f;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UHollowTargetStatusSource : public UInterface
{
	GENERATED_BODY()
};

/**
 * A tool target that shows its progress in the visor while a local player's tool works on it (doors now; seams,
 * bulkheads, cores later). Read on any machine from replicated state; never changes anything.
 */
class HOLLOWCORE_API IHollowTargetStatusSource
{
	GENERATED_BODY()

public:
	/** False if the target has nothing to show for this tool. */
	virtual bool GetTargetStatus(const FGameplayTag& ToolTag, FHollowTargetStatus& OutStatus) const = 0;
};
