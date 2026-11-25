#include "Damage/HollowDamageMath.h"

const TCHAR* LexToString(const EHollowHitKind Kind)
{
	switch (Kind)
	{
	case EHollowHitKind::Hit: return TEXT("Hit");
	case EHollowHitKind::Armor: return TEXT("Armor");
	case EHollowHitKind::Crit: return TEXT("Crit");
	default: return TEXT("None");
	}
}

EHollowHitKind HollowDamageMath::Classify(const float Base, const FHollowDamageOutcome& Outcome, const float ArmorShare)
{
	if (Base <= 0.f)
	{
		return EHollowHitKind::None;
	}
	if (Outcome.bImmune || Outcome.Damage <= 0.f)
	{
		return EHollowHitKind::Armor;
	}
	if (Outcome.bExposed)
	{
		return EHollowHitKind::Crit;
	}
	return 1.f - Outcome.Damage / Base >= ArmorShare ? EHollowHitKind::Armor : EHollowHitKind::Hit;
}

float HollowDamageMath::Mitigate(const float Base, const float Armor, const float ArmorConstant, const bool bArmorApplies, const bool bImmune,
	const float Penetration)
{
	if (bImmune || Base <= 0.f)
	{
		return 0.f;
	}
	if (!bArmorApplies || ArmorConstant <= 0.f)
	{
		return Base;
	}
	const float Effective = FMath::Max(Armor, 0.f) * (1.f - FMath::Clamp(Penetration, 0.f, 1.f));
	return Base * ArmorConstant / (ArmorConstant + Effective);
}
