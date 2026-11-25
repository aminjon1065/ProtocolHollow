#include "Feel/HollowFeelConfig.h"

#include "Misc/DataValidation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowFeelConfig)

const UHollowFeelConfig* UHollowFeelConfig::Get()
{
	return GetDefault<UHollowFeelSettings>()->Config.LoadSynchronous();
}

const FHollowFeelEntry* UHollowFeelConfig::Find(const FGameplayTag& Trigger) const
{
	return Entries.FindByPredicate([&Trigger](const FHollowFeelEntry& Entry) { return Entry.Trigger == Trigger; });
}

#if WITH_EDITOR
EDataValidationResult UHollowFeelConfig::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	for (const FString& Problem : HollowFeelRules::Check(Entries))
	{
		Context.AddError(FText::FromString(Problem));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif
