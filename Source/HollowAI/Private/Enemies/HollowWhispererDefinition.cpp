#include "Enemies/HollowWhispererDefinition.h"

#include "EnvironmentQuery/EnvQuery.h"
#include "GameplayEffect.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowWhispererDefinition)

#define LOCTEXT_NAMESPACE "HollowWhispererDefinition"

#if WITH_EDITOR
EDataValidationResult UHollowWhispererDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (!LitEffect || !FrenzyEffect || !FlankQuery)
	{
		Context.AddError(LOCTEXT("Missing", "A Whisperer needs its lit and frenzy effects and its flank query."));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
