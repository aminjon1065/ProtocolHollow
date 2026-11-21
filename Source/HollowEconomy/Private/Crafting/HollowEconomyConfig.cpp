#include "Crafting/HollowEconomyConfig.h"

#include "Profile/HollowEconomySettings.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowEconomyConfig)

#define LOCTEXT_NAMESPACE "HollowEconomyConfig"

const UHollowEconomyConfig* UHollowEconomyConfig::Get()
{
	return GetDefault<UHollowEconomySettings>()->EconomyConfig.LoadSynchronous();
}

#if WITH_EDITOR
EDataValidationResult UHollowEconomyConfig::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	TSet<FPrimaryAssetId> Seen;
	for (const FHollowShopOffer& Offer : Offers)
	{
		bool bDuplicate = false;
		Seen.Add(Offer.Item, &bDuplicate);
		if (!Offer.Item.IsValid() || Offer.Price <= 0 || bDuplicate)
		{
			Context.AddError(FText::Format(LOCTEXT("Offer", "{0}: every offer needs an item, once, and a price above 0 ({1})."), FText::FromName(GetFName()),
				FText::FromString(Offer.Item.ToString())));
			Result = EDataValidationResult::Invalid;
		}
	}
	if (SellPercent >= 100)
	{
		Context.AddError(FText::Format(LOCTEXT("SellPercent", "{0}: SellPercent must stay below 100, or buying to sell would pay."), FText::FromName(GetFName())));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
