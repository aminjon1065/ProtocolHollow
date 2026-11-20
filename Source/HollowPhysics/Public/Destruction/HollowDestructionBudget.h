#pragma once

#include "Breach/HollowBreachTypes.h"
#include "Engine/DeveloperSettings.h"

#include "HollowDestructionBudget.generated.h"

/** Cosmetic destruction limits of one machine at one quality level (Docs/Architecture/03 sec. 3.4). */
USTRUCT(BlueprintType)
struct FHollowDestructionBudget
{
	GENERATED_BODY()

	/** Zones fracturing live at the same time. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Budget", meta = (ClampMin = "0"))
	int32 MaxLiveBreaches = 0;

	/** Rigid bodies of all live fractures together. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Budget", meta = (ClampMin = "0"))
	int32 MaxActiveBodies = 0;

	/** Hard cap on a live fracture: its debris goes, the static rubble stays. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Budget", meta = (ClampMin = "0", Units = "Seconds"))
	float MaxDebrisLifetime = 0.f;

	/** Farther from the local camera than this, breaks snap. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Budget", meta = (ClampMin = "0", Units = "Centimeters"))
	float MaxSimDistance = 0.f;
};

/** What one machine knows when a zone breaks: inputs of the presentation policy. */
struct FHollowBreachPresentationInput
{
	/** Since the break, in server time (a client's estimate may run slightly behind: negative counts as zero). */
	double AgeSeconds = 0.0;
	float FreshWindowSeconds = 0.f;
	/** To the nearest local camera; unset where nobody watches (dedicated or headless server). */
	TOptional<double> ViewDistance;
	/** The zone was on screen a moment ago. */
	bool bRecentlyRendered = false;
	int32 LiveBreaches = 0;
	int32 LiveBodies = 0;
	/** Rigid bodies this fracture would add. */
	int32 NewBodies = 0;
};

/** Pure presentation policy (Docs/Architecture/03 sec. 3.4): Full only for a fresh break, near, seen and within budget. */
namespace HollowBreachPolicy
{
	HOLLOWPHYSICS_API EHollowBreachPresentation Choose(const FHollowBreachPresentationInput& Input, const FHollowDestructionBudget& Budget);
}

/** Destruction budgets per effects quality (Project Settings > Game > Hollow Destruction, DefaultGame.ini). */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Hollow Destruction"))
class HOLLOWPHYSICS_API UHollowDestructionBudgetConfig : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Budget for an effects quality level (sg.EffectsQuality: 0 Low .. 3 Epic, 4 Cinematic); clamped to the table. */
	FHollowDestructionBudget GetBudget(int32 EffectsQuality) const;

	/** Indexed by sg.EffectsQuality. */
	UPROPERTY(Config, EditAnywhere, Category = "Budget")
	TArray<FHollowDestructionBudget> BudgetsByEffectsQuality;

	/** A zone counts as seen if it was rendered within this many seconds. */
	UPROPERTY(Config, EditAnywhere, Category = "Visibility", meta = (ClampMin = "0", Units = "Seconds"))
	float RecentlyRenderedSeconds = 0.f;
};
