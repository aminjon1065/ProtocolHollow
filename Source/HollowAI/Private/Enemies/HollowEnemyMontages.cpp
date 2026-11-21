#include "Enemies/HollowEnemyMontages.h"

#include "Animation/AnimMontage.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowEnemyMontages)

const TSoftObjectPtr<UAnimMontage>& FHollowEnemyMontages::Get(const EHollowEnemyMontage Moment) const
{
	switch (Moment)
	{
	case EHollowEnemyMontage::Melee:     return Melee;
	case EHollowEnemyMontage::Breach:    return Breach;
	case EHollowEnemyMontage::Stunned:   return Stunned;
	case EHollowEnemyMontage::VentEnter: return VentEnter;
	case EHollowEnemyMontage::VentExit:  return VentExit;
	}
	return Melee;
}

TArray<FSoftObjectPath> FHollowEnemyMontages::Assigned() const
{
	TArray<FSoftObjectPath> Paths;
	for (const TSoftObjectPtr<UAnimMontage>* Slot : {&Melee, &Breach, &Stunned, &VentEnter, &VentExit})
	{
		if (!Slot->IsNull())
		{
			Paths.AddUnique(Slot->ToSoftObjectPath());
		}
	}
	return Paths;
}

float HollowEnemyMontages::PlayRateToLand(const UAnimMontage& Montage, const FName ImpactSection, const float Seconds)
{
	const int32 Section = Montage.GetSectionIndex(ImpactSection);
	if (Section == INDEX_NONE || Seconds <= 0.f)
	{
		return 1.f;
	}
	const float Impact = Montage.GetAnimCompositeSection(Section).GetTime();
	return Impact > 0.f ? FMath::Clamp(Impact / Seconds, MinPlayRate, MaxPlayRate) : 1.f;
}
