#include "Destruction/HollowDestructionBudget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowDestructionBudget)

EHollowBreachPresentation HollowBreachPolicy::Choose(const FHollowBreachPresentationInput& Input, const FHollowDestructionBudget& Budget)
{
	const bool bFresh = FMath::Max(Input.AgeSeconds, 0.0) < Input.FreshWindowSeconds;
	const bool bNear = Input.ViewDistance.IsSet() && Input.ViewDistance.GetValue() < Budget.MaxSimDistance;
	const bool bSlot = Input.LiveBreaches < Budget.MaxLiveBreaches;
	const bool bBodies = Input.LiveBodies + Input.NewBodies <= Budget.MaxActiveBodies;
	return bFresh && bNear && Input.bRecentlyRendered && bSlot && bBodies ? EHollowBreachPresentation::Full : EHollowBreachPresentation::Snap;
}

FHollowDestructionBudget UHollowDestructionBudgetConfig::GetBudget(const int32 EffectsQuality) const
{
	if (BudgetsByEffectsQuality.IsEmpty())
	{
		return FHollowDestructionBudget();   // no budget: everything snaps
	}
	return BudgetsByEffectsQuality[FMath::Clamp(EffectsQuality, 0, BudgetsByEffectsQuality.Num() - 1)];
}
