#include "Damage/HollowDamageMath.h"

float HollowDamageMath::Mitigate(const float Base, const float Armor, const float ArmorConstant, const bool bArmorApplies, const bool bImmune)
{
	if (bImmune || Base <= 0.f)
	{
		return 0.f;
	}
	if (!bArmorApplies || ArmorConstant <= 0.f)
	{
		return Base;
	}
	return Base * ArmorConstant / (ArmorConstant + FMath::Max(Armor, 0.f));
}
