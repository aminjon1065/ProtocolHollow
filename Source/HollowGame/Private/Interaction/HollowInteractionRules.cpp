#include "Interaction/HollowInteractionRules.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowInteractionRules)

namespace HollowInteractionRules
{
	EHollowInteractionRejection ValidateRequest(const FHollowInteractionCheck& Check, const float MaxDistance, const float MinInterval)
	{
		if (!Check.bTargetValid)
		{
			return EHollowInteractionRejection::NoTarget;
		}
		if (!Check.bTargetInteractable)
		{
			return EHollowInteractionRejection::NotInteractable;
		}
		if (Check.bInteractorBlocked)
		{
			return EHollowInteractionRejection::InteractorBlocked;
		}
		if (Check.SecondsSinceLastInteraction.IsSet() && Check.SecondsSinceLastInteraction.GetValue() < MinInterval)
		{
			return EHollowInteractionRejection::TooSoon;
		}
		if (!Check.bHasLineOfSight)
		{
			return EHollowInteractionRejection::NoLineOfSight;
		}
		if (Check.Distance > MaxDistance)
		{
			return EHollowInteractionRejection::OutOfRange;
		}
		if (!Check.bTargetAllows)
		{
			return EHollowInteractionRejection::TargetRefused;
		}
		return EHollowInteractionRejection::None;
	}
}
