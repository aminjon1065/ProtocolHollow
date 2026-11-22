#pragma once

#include "CoreMinimal.h"

#include "HollowArcRules.generated.h"

/** Where an electric arc is in its cycle (S7-02, ADR-0039). A powered cycle starts with the warning. */
UENUM()
enum class EHollowArcPhase : uint8
{
	/** The breaker is off: nothing. */
	Off,
	/** Warning: the emitters glow and crackle; step out now. */
	Telegraph,
	/** The arc strikes: whoever is inside when it starts takes the damage. */
	Discharge,
	/** Quiet until the next warning. */
	Idle
};

/** The cycle's lengths (UHollowArcHazardDefinition). */
struct FHollowArcTimings
{
	float TelegraphSeconds = 0.f;
	float DischargeSeconds = 0.f;
	float IdleSeconds = 0.f;

	double Period() const { return static_cast<double>(TelegraphSeconds) + DischargeSeconds + IdleSeconds; }
};

/** A moment of the cycle: its phase and how long until the next one starts. */
struct FHollowArcMoment
{
	EHollowArcPhase Phase = EHollowArcPhase::Off;
	double SecondsToNext = 0.0;
};

/** Pure cycle arithmetic of the arcs (every machine runs it on the server's clock; tests). */
namespace HollowArcRules
{
	/** The moment SinceCycleStart seconds after the power came on (Telegraph, Discharge, Idle, again). */
	HOLLOWGAME_API FHollowArcMoment MomentAt(double SinceCycleStart, const FHollowArcTimings& Timings, bool bPowered);
	/** Whether the timings make a cycle (every phase longer than zero). */
	HOLLOWGAME_API bool IsValid(const FHollowArcTimings& Timings);
}
