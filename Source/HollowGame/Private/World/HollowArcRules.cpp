#include "World/HollowArcRules.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowArcRules)

bool HollowArcRules::IsValid(const FHollowArcTimings& Timings)
{
	return Timings.TelegraphSeconds > 0.f && Timings.DischargeSeconds > 0.f && Timings.IdleSeconds > 0.f;
}

FHollowArcMoment HollowArcRules::MomentAt(const double SinceCycleStart, const FHollowArcTimings& Timings, const bool bPowered)
{
	FHollowArcMoment Moment;
	if (!bPowered || !IsValid(Timings))
	{
		return Moment;   // Off, nothing to wait for
	}
	const double Period = Timings.Period();
	const double InCycle = FMath::Fmod(FMath::Max(SinceCycleStart, 0.0), Period);
	const double DischargeFrom = Timings.TelegraphSeconds;
	const double IdleFrom = DischargeFrom + Timings.DischargeSeconds;
	if (InCycle < DischargeFrom)
	{
		Moment.Phase = EHollowArcPhase::Telegraph;
		Moment.SecondsToNext = DischargeFrom - InCycle;
	}
	else if (InCycle < IdleFrom)
	{
		Moment.Phase = EHollowArcPhase::Discharge;
		Moment.SecondsToNext = IdleFrom - InCycle;
	}
	else
	{
		Moment.Phase = EHollowArcPhase::Idle;
		Moment.SecondsToNext = Period - InCycle;
	}
	return Moment;
}
