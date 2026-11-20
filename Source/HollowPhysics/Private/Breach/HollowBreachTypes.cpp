#include "Breach/HollowBreachTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowBreachTypes)

namespace HollowBreachTypesPrivate
{
	/** State in the low bits, the zone mask above: both fit one byte. */
	constexpr uint8 StateBits = 2;
	constexpr uint8 StateMask = (1u << StateBits) - 1u;
	static_assert(static_cast<uint8>(EHollowBreachState::Breached) <= StateMask, "breach states must fit StateBits");
	static_assert(StateBits + FHollowBreachNetState::MaxZones <= 8, "state and zone mask must fit one byte");

	constexpr double ByteSteps = 256.0;          // yaw wraps around: 256 steps of 360 / 256 degrees
	constexpr double PitchSteps = 255.0;         // pitch spans [-90, 90] inclusive
	constexpr double FullTurn = 360.0;
	constexpr double QuarterTurn = 90.0;
}

bool FHollowBreachNetState::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	using namespace HollowBreachTypesPrivate;
	uint8 StateAndZones = static_cast<uint8>(static_cast<uint8>(State) | (ZoneMask << StateBits));
	Ar << StateAndZones;
	Ar << DamageStageQ;
	Ar << ImpactYawQ;
	Ar << ImpactPitchQ;
	Ar << BreachServerTime;
	Ar << TransitionSerial;
	if (Ar.IsLoading())
	{
		State = static_cast<EHollowBreachState>(FMath::Min<uint8>(StateAndZones & StateMask, static_cast<uint8>(EHollowBreachState::Breached)));
		ZoneMask = static_cast<uint8>(StateAndZones >> StateBits);
	}
	bOutSuccess = true;
	return true;
}

FVector FHollowBreachNetState::GetImpactDirection() const
{
	using namespace HollowBreachTypesPrivate;
	const double Yaw = ImpactYawQ * FullTurn / ByteSteps;
	const double Pitch = ImpactPitchQ * (2.0 * QuarterTurn) / PitchSteps - QuarterTurn;
	return FRotator(Pitch, Yaw, 0.0).Vector();
}

void FHollowBreachNetState::SetImpactDirection(const FVector& Direction)
{
	using namespace HollowBreachTypesPrivate;
	const FRotator Rotation = Direction.GetSafeNormal().Rotation();
	ImpactYawQ = static_cast<uint8>(FMath::RoundToInt(FRotator::ClampAxis(Rotation.Yaw) * ByteSteps / FullTurn) & 0xFF);
	ImpactPitchQ = static_cast<uint8>(FMath::RoundToInt((FMath::Clamp(Rotation.Pitch, -QuarterTurn, QuarterTurn) + QuarterTurn) * PitchSteps / (2.0 * QuarterTurn)));
}
