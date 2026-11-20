#include "Resonance/HollowResonancePresentationConfig.h"

#include "Materials/MaterialParameterCollection.h"
#include "SoundControlBus.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowResonancePresentationConfig)

#define LOCTEXT_NAMESPACE "HollowResonancePresentationConfig"

const UHollowResonancePresentationConfig* UHollowResonancePresentationConfig::Get()
{
	return GetDefault<UHollowResonancePresentationSettings>()->Presentation.LoadSynchronous();
}

#if WITH_EDITOR
EDataValidationResult UHollowResonancePresentationConfig::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	const UMaterialParameterCollection* Loaded = Collection.LoadSynchronous();
	if (!Loaded || !Loaded->GetScalarParameterByName(IntensityParameter) || !Loaded->GetScalarParameterByName(TimeOffsetParameter))
	{
		Context.AddError(LOCTEXT("Collection", "The collection must hold the intensity and time offset scalars."));
		Result = EDataValidationResult::Invalid;
	}
	if (Bus.IsNull())
	{
		Context.AddError(LOCTEXT("Bus", "The Resonance bus is missing."));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
