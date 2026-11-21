#pragma once

#include "EditorValidatorBase.h"

#include "HollowArtBudgetValidator.generated.h"

/**
 * Data Validation of the art-pass budget (ADR-0027, UHollowArtBudgetSettings), when an asset is saved or validated:
 * a texture under the scanned paths that cooks larger than the budget, and in a room level decoration that collides
 * or can shape the navmesh. Warnings in the editor; Hollow.Editor.ArtBudget fails a strict run on them.
 */
UCLASS()
class HOLLOWEDITOR_API UHollowArtBudgetValidator : public UEditorValidatorBase
{
	GENERATED_BODY()

protected:
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context) override;
};
