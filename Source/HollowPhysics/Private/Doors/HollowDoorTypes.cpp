#include "Doors/HollowDoorTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowDoorTypes)

namespace HollowDoorTypesPrivate
{
	/** State in the low bits, flags above: both fit one byte (6 states, 3 flags). */
	constexpr uint8 StateBits = 3;
	constexpr uint8 StateMask = (1u << StateBits) - 1u;
	static_assert(static_cast<uint8>(EHollowDoorState::Breached) <= StateMask, "door states must fit StateBits");
}

bool FHollowDoorNetState::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	using namespace HollowDoorTypesPrivate;
	uint8 StateAndFlags = static_cast<uint8>(static_cast<uint8>(State) | (Flags << StateBits));
	Ar << StateAndFlags;
	Ar << WeldQ;
	Ar << StructureQ;
	Ar << TransitionSerial;
	Ar << StateServerTime;
	if (Ar.IsLoading())
	{
		State = static_cast<EHollowDoorState>(FMath::Min<uint8>(StateAndFlags & StateMask, static_cast<uint8>(EHollowDoorState::Breached)));
		Flags = static_cast<uint8>(StateAndFlags >> StateBits);
	}
	bOutSuccess = true;
	return true;
}
