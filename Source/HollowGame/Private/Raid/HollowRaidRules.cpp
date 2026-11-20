#include "Raid/HollowRaidRules.h"

namespace HollowRaidRules
{
	EHollowCountdownStep StepCountdown(const bool bCounting, const double Now, const double DepartureTime, const int32 LivingInZone)
	{
		if (!bCounting)
		{
			return LivingInZone > 0 ? EHollowCountdownStep::Start : EHollowCountdownStep::None;
		}
		if (LivingInZone <= 0)
		{
			return EHollowCountdownStep::Reset;
		}
		return Now >= DepartureTime ? EHollowCountdownStep::Depart : EHollowCountdownStep::None;
	}

	EHollowRaidOutcome OutcomeAtDeparture(const EHollowRaidOutcome Current, const bool bInZone)
	{
		if (Current != EHollowRaidOutcome::None)
		{
			return Current;   // dead or gone before the shuttle left
		}
		return bInZone ? EHollowRaidOutcome::Extracted : EHollowRaidOutcome::MissingInAction;
	}

	bool IsOver(const int32 LivingPlayers)
	{
		return LivingPlayers <= 0;
	}
}
