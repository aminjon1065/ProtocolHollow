#include "Gadgets/HollowOccultWardDefinition.h"

#include "Effects/HollowOccultEffects.h"
#include "GameplayEffect.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowOccultWardDefinition)

#define LOCTEXT_NAMESPACE "HollowOccultWardDefinition"

#if WITH_EDITOR
EDataValidationResult UHollowOccultWardDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	auto Fail = [&Context, &Result](const FText& Message)
	{
		Context.AddError(Message);
		Result = EDataValidationResult::Invalid;
	};
	if (!EnemyEffect || !EnemyEffect->IsChildOf<UHollowWardBindEffect>())
	{
		Fail(LOCTEXT("Enemy", "A ward's EnemyEffect must be a UHollowWardBindEffect (GE_Ward_Bind)."));
	}
	if (!PlayerEffect || !PlayerEffect->IsChildOf<UHollowWardCalmEffect>())
	{
		Fail(LOCTEXT("Player", "A ward's PlayerEffect must be a UHollowWardCalmEffect (GE_Ward_Calm)."));
	}
	if (!DrainEffect || DrainEffect.GetDefaultObject()->DurationPolicy != EGameplayEffectDurationType::Instant)
	{
		Fail(LOCTEXT("Drain", "A ward's DrainEffect must be instant (GE_Ward_Drain)."));
	}
	if (CheckSeconds > LifeSeconds || FadeInSeconds + PulseSeconds > LifeSeconds)
	{
		Fail(LOCTEXT("Times", "A ward checks, comes up and pulses within its life."));
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
