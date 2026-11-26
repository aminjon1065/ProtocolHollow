#include "Movement/HollowMovementTuning.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowMovementTuning)

const FHollowCarryModeTuning* FHollowCarryMovementTuning::Find(const EHollowCarryClaim Claim) const
{
	switch (Claim)
	{
	case EHollowCarryClaim::Solo:
		return &Solo;
	case EHollowCarryClaim::Coop:
		return &Coop;
	case EHollowCarryClaim::Dragged:
		return &Dragged;
	case EHollowCarryClaim::Drone:
		return &Drone;
	case EHollowCarryClaim::None:
		break;
	}
	return nullptr;
}

float FHollowCarryMovementTuning::GetSpeedScale(const EHollowCarryClaim Claim) const
{
	const FHollowCarryModeTuning* Mode = Find(Claim);
	return Mode ? Mode->SpeedScale : 1.f;
}
