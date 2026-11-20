#include "Gadgets/HollowPulseMineDefinition.h"

#include "GameplayEffect.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowPulseMineDefinition)

#define LOCTEXT_NAMESPACE "HollowPulseMineDefinition"

#if WITH_EDITOR
EDataValidationResult UHollowPulseMineDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (!StunEffect)
	{
		Context.AddError(LOCTEXT("Stun", "A pulse mine needs its stun effect."));
		Result = EDataValidationResult::Invalid;
	}
	if (BlastRadius < TriggerRadius)
	{
		Context.AddError(LOCTEXT("Radius", "The blast must reach at least as far as the trigger."));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
