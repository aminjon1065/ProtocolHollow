#include "Data/HollowLanternDefinition.h"

#include "Effects/HollowOccultEffects.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowLanternDefinition)

#define LOCTEXT_NAMESPACE "HollowLanternDefinition"

#if WITH_EDITOR
EDataValidationResult UHollowLanternDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	const FText Name = FText::FromName(GetFName());
	if (!ExposeEffect || !ExposeEffect->IsChildOf<UHollowOccultExposeEffect>())
	{
		Context.AddError(FText::Format(LOCTEXT("Expose", "{0}: ExposeEffect must be a UHollowOccultExposeEffect (GE_Occult_Expose)."), Name));
		Result = EDataValidationResult::Invalid;
	}
	if (!AuraEffect || !AuraEffect->IsChildOf<UHollowAntiResonanceEffect>())
	{
		Context.AddError(FText::Format(LOCTEXT("Aura", "{0}: AuraEffect must be a UHollowAntiResonanceEffect (GE_AntiResonance)."), Name));
		Result = EDataValidationResult::Invalid;
	}
	if (ConeHalfAngleDegrees <= 0.f || Range <= 0.f || AuraRadius <= 0.f || SampleRateHz <= 0.f)
	{
		Context.AddError(FText::Format(LOCTEXT("Shape", "{0}: the cone (angle, range), the aura radius and the sample rate must be positive."), Name));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
