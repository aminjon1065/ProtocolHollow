#include "Resonance/HollowResonanceConfig.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowResonanceConfig)

#define LOCTEXT_NAMESPACE "HollowResonanceConfig"

const UHollowResonanceConfig* UHollowResonanceConfig::Get()
{
	return GetDefault<UHollowResonanceSettings>()->Config.LoadSynchronous();
}

#if WITH_EDITOR
EDataValidationResult UHollowResonanceConfig::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	bool bAscending = TierThresholds.Num() == 3;
	for (int32 Index = 1; Index < TierThresholds.Num(); ++Index)
	{
		bAscending &= TierThresholds[Index] - TierThresholds[Index - 1] > Hysteresis;
	}
	if (!bAscending)
	{
		Context.AddError(LOCTEXT("Thresholds", "Three ascending tier thresholds (Medium, High, Critical), further apart than the hysteresis."));
		Result = EDataValidationResult::Invalid;
	}
	if (UpdateSeconds <= 0.f)
	{
		Context.AddError(LOCTEXT("Update", "The server needs an update interval."));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
