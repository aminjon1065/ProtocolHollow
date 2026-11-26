#include "Carry/HollowCarryTypes.h"

#include "GameFramework/Pawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowCarryTypes)

namespace HollowCarry
{
	EHollowCarryClaim ClaimFor(const EHollowCarryMode Mode)
	{
		switch (Mode)
		{
		case EHollowCarryMode::Solo:
			return EHollowCarryClaim::Solo;
		case EHollowCarryMode::Drone:
			return EHollowCarryClaim::Drone;
		case EHollowCarryMode::Coop:
			return EHollowCarryClaim::Coop;
		case EHollowCarryMode::Dragged:
			return EHollowCarryClaim::Dragged;
		case EHollowCarryMode::None:
		case EHollowCarryMode::Winched:
			break;
		}
		return EHollowCarryClaim::None;
	}
}
