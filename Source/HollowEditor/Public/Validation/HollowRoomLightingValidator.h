#pragma once

#include "EditorValidatorBase.h"

#include "HollowRoomLightingValidator.generated.h"

class UHollowRoomDefinition;
class UWorld;

/**
 * Data Validation of a room's level (ADR-0021): when a room shell is saved or validated, its lights must fit the
 * budget of its room type (DA_LightBudget: lights, shadowed, shadowed and flickering, light functions, volumetric
 * shadows, reach). The room type comes from the UHollowRoomDefinition naming this level.
 */
UCLASS()
class HOLLOWEDITOR_API UHollowRoomLightingValidator : public UEditorValidatorBase
{
	GENERATED_BODY()

public:
	/** The room definition whose RoomLevel is this world, or null. */
	static const UHollowRoomDefinition* FindRoomFor(const UWorld& World);

protected:
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context) override;
};
