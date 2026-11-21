#include "Crafting/HollowCraftingRecipe.h"

#include "Crafting/HollowCraftingRules.h"
#include "Engine/AssetManager.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowCraftingRecipe)

#define LOCTEXT_NAMESPACE "HollowCraftingRecipe"

const FPrimaryAssetType UHollowCraftingRecipe::AssetType(TEXT("HollowRecipe"));

FPrimaryAssetId UHollowCraftingRecipe::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, GetFName());
}

FHollowRecipeTerms UHollowCraftingRecipe::ToTerms() const
{
	FHollowRecipeTerms Terms;
	Terms.Id = GetPrimaryAssetId();
	Terms.Inputs = Inputs;
	Terms.Credits = Credits;
	Terms.Output = Output;
	return Terms;
}

TArray<FHollowRecipeTerms> UHollowCraftingRecipe::LoadAll()
{
	TArray<FHollowRecipeTerms> All;
	if (!UAssetManager::IsInitialized())
	{
		return All;
	}
	TArray<FPrimaryAssetId> Ids;
	UAssetManager::Get().GetPrimaryAssetIdList(AssetType, Ids);
	Ids.Sort([](const FPrimaryAssetId& A, const FPrimaryAssetId& B) { return A.ToString() < B.ToString(); });
	for (const FPrimaryAssetId& Id : Ids)
	{
		if (const UHollowCraftingRecipe* Recipe = Cast<UHollowCraftingRecipe>(UAssetManager::Get().GetPrimaryAssetPath(Id).TryLoad()))
		{
			All.Add(Recipe->ToTerms());
		}
	}
	return All;
}

#if WITH_EDITOR
EDataValidationResult UHollowCraftingRecipe::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	bool bValid = !DisplayName.IsEmpty() && Output.Item.IsValid() && Output.Count > 0 && !Inputs.IsEmpty() && Credits >= 0;
	for (const FHollowItemStack& Input : Inputs)
	{
		bValid &= Input.Item.IsValid() && Input.Count > 0 && Input.Item != Output.Item;
	}
	if (!bValid)
	{
		Context.AddError(FText::Format(LOCTEXT("Recipe", "{0}: a recipe needs a name, inputs (each an item and a count, none its own output) and an output."),
			FText::FromName(GetFName())));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
