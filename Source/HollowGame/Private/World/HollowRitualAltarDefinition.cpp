#include "World/HollowRitualAltarDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowRitualAltarDefinition)

#define LOCTEXT_NAMESPACE "HollowRitualAltarDefinition"

#if WITH_EDITOR
EDataValidationResult UHollowRitualAltarDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	auto Fail = [&Context, &Result](const FText& Message)
	{
		Context.AddError(Message);
		Result = EDataValidationResult::Invalid;
	};
	if (!AcceptedItem.IsValid() || !ResultItem.IsValid() || AcceptedItem == ResultItem)
	{
		Fail(LOCTEXT("Items", "An altar turns one item (AcceptedItem) into another (ResultItem)."));
	}
	if (RitualSeconds <= 0.f || PulseSeconds <= 0.f || PulseSeconds > RitualSeconds)
	{
		Fail(LOCTEXT("Times", "A ritual lasts RitualSeconds and tolls at least once (PulseSeconds within it)."));
	}
	if (!NoiseTag.IsValid())
	{
		Fail(LOCTEXT("Noise", "An altar's tolls need a noise tag (Hollow.Noise.Ritual)."));
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
