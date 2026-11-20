#include "Carry/HollowCarryableDefinition.h"

#include "GameplayEffect.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowCarryableDefinition)

#define LOCTEXT_NAMESPACE "HollowCarryableDefinition"

const FPrimaryAssetType UHollowCarryableDefinition::AssetType(TEXT("HollowCarryable"));

FPrimaryAssetId UHollowCarryableDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, GetFName());
}

#if WITH_EDITOR
EDataValidationResult UHollowCarryableDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	auto Error = [&Context, &Result](const FText& Message)
	{
		Context.AddError(Message);
		Result = EDataValidationResult::Invalid;
	};
	if (!SoloEffect)
	{
		Error(LOCTEXT("SoloEffect", "A carryable needs the effect applied to its solo carrier."));
	}
	if (!PhysicalMaterial)
	{
		Error(LOCTEXT("PhysicalMaterial", "A carryable needs a physical material (friction, sleep thresholds)."));
	}
	if (MaxAwakeSeconds <= DormancyDelaySeconds)
	{
		Error(LOCTEXT("Timing", "MaxAwakeSeconds must exceed DormancyDelaySeconds."));
	}
	if (PickupPrompt.IsEmpty())
	{
		Error(LOCTEXT("Prompt", "A carryable needs a pick-up prompt."));
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
