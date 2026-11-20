#pragma once

#include "Extraction/HollowRaidTypes.h"

/** What the extraction countdown does on one poll. */
enum class EHollowCountdownStep : uint8
{
	None,
	Start,
	Reset,
	Depart
};

/** Pure raid rules (ADR-0016), tested by Hollow.Game.Raid.Rules. */
namespace HollowRaidRules
{
	/**
	 * The shuttle counts down while someone alive is in the zone: it starts when one gets in, resets when the zone is
	 * empty again (that wins over a departure due at the same poll) and departs at DepartureTime (inclusive).
	 */
	HOLLOWGAME_API EHollowCountdownStep StepCountdown(bool bCounting, double Now, double DepartureTime, int32 LivingInZone);

	/** A player's raid at departure: aboard (alive in the zone), left behind (alive outside) or already decided. */
	HOLLOWGAME_API EHollowRaidOutcome OutcomeAtDeparture(EHollowRaidOutcome Current, bool bInZone);

	/** The raid ends once nobody is left to extract. */
	HOLLOWGAME_API bool IsOver(int32 LivingPlayers);
}
