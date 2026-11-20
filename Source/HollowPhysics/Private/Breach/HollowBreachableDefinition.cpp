#include "Breach/HollowBreachableDefinition.h"

#include "Breach/HollowBreachTypes.h"
#include "Nav/HollowNavAreas.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowBreachableDefinition)

#define LOCTEXT_NAMESPACE "HollowBreachableDefinition"

const FPrimaryAssetType UHollowBreachableDefinition::AssetType(TEXT("HollowBreachable"));

UHollowBreachableDefinition::UHollowBreachableDefinition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The Hollow areas are the structural default; a variant may point at its own area classes.
	AreaIntact = UHollowNavArea_BulkheadIntact::StaticClass();
	AreaBreached = UHollowNavArea_Doorway::StaticClass();
}

FPrimaryAssetId UHollowBreachableDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, GetFName());
}

float UHollowBreachableDefinition::GetTotalHealth() const
{
	float Total = 0.f;
	for (const FHollowBreachZone& Zone : Zones)
	{
		Total += Zone.MaxHealth;
	}
	return Total;
}

uint8 UHollowBreachableDefinition::GetPassageMask() const
{
	uint8 Mask = 0;
	for (int32 Index = 0; Index < FMath::Min(Zones.Num(), FHollowBreachNetState::MaxZones); ++Index)
	{
		Mask |= Zones[Index].bRequiredForPassage ? static_cast<uint8>(1u << Index) : uint8(0);
	}
	return Mask;
}

#if WITH_EDITOR
EDataValidationResult UHollowBreachableDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	auto Error = [&Context, &Result](const FText& Message)
	{
		Context.AddError(Message);
		Result = EDataValidationResult::Invalid;
	};
	if (Zones.IsEmpty() || Zones.Num() > FHollowBreachNetState::MaxZones)
	{
		Error(FText::Format(LOCTEXT("ZoneCount", "A bulkhead needs 1 to {0} zones."), FHollowBreachNetState::MaxZones));
	}
	if (GetPassageMask() == 0)
	{
		Error(LOCTEXT("NoPassage", "At least one zone must be required for the passage."));
	}
	for (int32 Index = 0; Index < Zones.Num(); ++Index)
	{
		const FHollowBreachZone& Zone = Zones[Index];
		if (Zone.MaxHealth <= 0.f || Zone.HalfSize.GetMin() <= 0.f)
		{
			Error(FText::Format(LOCTEXT("ZoneData", "Zone {0} needs positive health and size."), Index));
		}
		if (Zone.Fracture.IsNull())
		{
			Error(FText::Format(LOCTEXT("ZoneFracture", "Zone {0} has no fracture geometry collection."), Index));
		}
	}
	if (!AreaIntact || !AreaBreached)
	{
		Error(LOCTEXT("Areas", "Both nav areas must be set."));
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif

#undef LOCTEXT_NAMESPACE
