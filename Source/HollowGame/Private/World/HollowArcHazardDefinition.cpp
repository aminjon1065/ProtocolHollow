#include "World/HollowArcHazardDefinition.h"

#include "GameplayEffect.h"
#include "World/HollowSwitch.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowArcHazardDefinition)

#define LOCTEXT_NAMESPACE "HollowArcHazardDefinition"

#if WITH_EDITOR
EDataValidationResult UHollowArcHazardDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	auto Fail = [&Context, &Result](const FText& Message)
	{
		Context.AddError(Message);
		Result = EDataValidationResult::Invalid;
	};
	if (!HollowArcRules::IsValid(GetTimings()))
	{
		Fail(LOCTEXT("Cycle", "An arc warns, strikes and rests: every phase lasts longer than zero."));
	}
	if (!DamageEffect || DamageEffect.GetDefaultObject()->DurationPolicy != EGameplayEffectDurationType::Instant || Damage <= 0.f)
	{
		Fail(LOCTEXT("Damage", "An arc hurts: an instant DamageEffect (GE_Damage_Electric) and a Damage above zero."));
	}
	if (!BreakerClass || ZoneExtent.GetMin() <= 0.0)
	{
		Fail(LOCTEXT("Breaker", "An arc has a zone and a breaker (BP_ArcBreaker)."));
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
