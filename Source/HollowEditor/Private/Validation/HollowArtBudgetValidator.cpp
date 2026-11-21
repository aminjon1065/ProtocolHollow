#include "Validation/HollowArtBudgetValidator.h"

#include "Engine/Texture.h"
#include "Engine/World.h"
#include "Validation/HollowArtBudget.h"
#include "Validation/HollowRoomLightingValidator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowArtBudgetValidator)

#define LOCTEXT_NAMESPACE "HollowArtBudgetValidator"

bool UHollowArtBudgetValidator::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const
{
	const UHollowArtBudgetSettings* Settings = GetDefault<UHollowArtBudgetSettings>();
	if (const UTexture* Texture = Cast<UTexture>(InObject))
	{
		return HollowArtBudget::IsScanned(Texture->GetOutermost()->GetName(), Settings->ScannedPaths);
	}
	const UWorld* World = Cast<UWorld>(InObject);
	return World && UHollowRoomLightingValidator::FindRoomFor(*World);
}

EDataValidationResult UHollowArtBudgetValidator::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context)
{
	const UHollowArtBudgetSettings* Settings = GetDefault<UHollowArtBudgetSettings>();
	TArray<FString> Problems;
	if (const UTexture* Texture = Cast<UTexture>(InAsset))
	{
		Problems = HollowArtBudget::CheckTexture(*Texture, Settings->MaxTextureSize);
	}
	else if (const UWorld* World = Cast<UWorld>(InAsset); World && World->PersistentLevel)
	{
		Problems = HollowArtBudget::CheckDecor(*World->PersistentLevel, Settings->DecorFolders);
	}
	for (const FString& Problem : Problems)
	{
		AssetWarning(InAsset, FText::Format(LOCTEXT("Budget", "Art budget: {0}."), FText::FromString(Problem)));
	}
	AssetPasses(InAsset);   // warnings, not errors: the asset saves; a strict test run refuses them
	return EDataValidationResult::Valid;
}

#undef LOCTEXT_NAMESPACE
