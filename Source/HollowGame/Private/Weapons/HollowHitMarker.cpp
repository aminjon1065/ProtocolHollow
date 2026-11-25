#include "Weapons/HollowHitMarker.h"

#include "AbilitySystemGlobals.h"
#include "Character/HollowExoFrameCharacter.h"
#include "Damage/HollowDamageExecution.h"
#include "Damage/HollowDamageSettings.h"
#include "Interfaces/HollowBreakableProp.h"

namespace HollowHitMarkerPrivate
{
	int32 Rank(const EHollowHitKind Kind)
	{
		switch (Kind)
		{
		case EHollowHitKind::Crit: return 3;
		case EHollowHitKind::Hit: return 2;
		case EHollowHitKind::Armor: return 1;
		default: return 0;
		}
	}
}

EHollowHitKind HollowHitMarker::KindOf(const AActor* Target, const TSubclassOf<UGameplayEffect> DamageEffect, const float Damage, const float Penetration,
	const float ExposedPenetration)
{
	if (!Target || Damage <= 0.f)
	{
		return EHollowHitKind::None;
	}
	if (Cast<AHollowExoFrameCharacter>(Target) && !GetDefault<UHollowDamageSettings>()->bFriendlyFire)
	{
		return EHollowHitKind::None;   // the pellet stopped at a squad mate, harmless
	}
	if (const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target))
	{
		const UHollowDamageEffect* Effect = DamageEffect ? Cast<UHollowDamageEffect>(DamageEffect.GetDefaultObject()) : nullptr;
		const EHollowHitKind Kind = HollowDamageRules::Preview(AbilitySystem, Effect ? Effect->DamageType : FGameplayTag(), Damage, Penetration, ExposedPenetration);
		if (Kind != EHollowHitKind::None)
		{
			return Kind;
		}
	}
	return Cast<IHollowBreakableProp>(Target) ? EHollowHitKind::Hit : EHollowHitKind::None;
}

EHollowHitKind HollowHitMarker::Strongest(const EHollowHitKind A, const EHollowHitKind B)
{
	return HollowHitMarkerPrivate::Rank(A) >= HollowHitMarkerPrivate::Rank(B) ? A : B;
}
