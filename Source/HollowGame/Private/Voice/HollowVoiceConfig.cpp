#include "Voice/HollowVoiceConfig.h"

#include "InputAction.h"
#include "InputMappingContext.h"
#include "Misc/DataValidation.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundEffectSource.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowVoiceConfig)

#if WITH_EDITOR
EDataValidationResult UHollowVoiceConfig::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	auto Require = [&Context, &Result](const bool bSet, const TCHAR* What)
	{
		if (!bSet)
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("%s is not set."), What)));
			Result = EDataValidationResult::Invalid;
		}
	};
	Require(PushToTalkAction != nullptr, TEXT("PushToTalkAction"));
	Require(MappingContext != nullptr, TEXT("MappingContext"));
	Require(Attenuation != nullptr, TEXT("Attenuation"));
	Require(HelmetChain != nullptr, TEXT("HelmetChain"));
	Require(RadioChain != nullptr, TEXT("RadioChain"));
	return Result;
}
#endif
