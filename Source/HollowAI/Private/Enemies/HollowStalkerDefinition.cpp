#include "Enemies/HollowStalkerDefinition.h"

#include "EnvironmentQuery/EnvQuery.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "Resonance/HollowResonanceMath.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowStalkerDefinition)

#define LOCTEXT_NAMESPACE "HollowStalkerDefinition"

#if WITH_EDITOR
EDataValidationResult UHollowStalkerDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (!FocusQuery)
	{
		Context.AddError(LOCTEXT("Query", "A stalker needs its focus query (EQ_StalkerFocusRoom)."));
		Result = EDataValidationResult::Invalid;
	}
	FString Problem;
	if (const FRichCurve* Curve = HearingResonanceCurve.GetRichCurveConst(); !Curve || !HollowResonanceMath::IsValidMultiplierCurve(*Curve, 1.f, Problem))
	{
		Context.AddError(FText::Format(LOCTEXT("Hearing", "HearingResonanceCurve: {0}."), FText::FromString(Curve ? Problem : FString(TEXT("missing")))));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
