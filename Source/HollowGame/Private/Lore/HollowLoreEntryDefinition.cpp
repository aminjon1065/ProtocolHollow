#include "Lore/HollowLoreEntryDefinition.h"

#include "Engine/AssetManager.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowLoreEntryDefinition)

#define LOCTEXT_NAMESPACE "HollowLoreEntryDefinition"

const FPrimaryAssetType UHollowLoreEntryDefinition::AssetType(TEXT("HollowLore"));

FPrimaryAssetId UHollowLoreEntryDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, GetFName());
}

TArray<const UHollowLoreEntryDefinition*> UHollowLoreEntryDefinition::LoadAll()
{
	TArray<FPrimaryAssetId> Ids;
	if (UAssetManager* Manager = UAssetManager::GetIfInitialized())
	{
		Manager->GetPrimaryAssetIdList(AssetType, Ids);
	}
	TArray<const UHollowLoreEntryDefinition*> Entries;
	for (const FPrimaryAssetId& Id : Ids)
	{
		if (const UHollowLoreEntryDefinition* Entry = Load(Id))
		{
			Entries.Add(Entry);
		}
	}
	Entries.Sort([](const UHollowLoreEntryDefinition& A, const UHollowLoreEntryDefinition& B)
	{
		return A.Number != B.Number ? A.Number < B.Number : A.GetFName().LexicalLess(B.GetFName());
	});
	return Entries;
}

const UHollowLoreEntryDefinition* UHollowLoreEntryDefinition::Load(const FPrimaryAssetId& Id)
{
	UAssetManager* Manager = Id.PrimaryAssetType == AssetType ? UAssetManager::GetIfInitialized() : nullptr;
	const FSoftObjectPath Path = Manager ? Manager->GetPrimaryAssetPath(Id) : FSoftObjectPath();
	return Path.IsValid() ? Cast<UHollowLoreEntryDefinition>(Path.TryLoad()) : nullptr;
}

#if WITH_EDITOR
EDataValidationResult UHollowLoreEntryDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	const FText Name = FText::FromName(GetFName());
	if (SectorFeature.IsEmpty() || SectorName.IsEmpty() || Number < 1)
	{
		Context.AddError(FText::Format(LOCTEXT("Place", "{0}: a crew log needs its sector Game Feature, the sector's name and its number (1 and up)."), Name));
		Result = EDataValidationResult::Invalid;
	}
	if (Title.IsEmpty() || Author.IsEmpty() || Stamp.IsEmpty() || Body.IsEmpty())
	{
		Context.AddError(FText::Format(LOCTEXT("Texts", "{0}: a crew log needs its title, author, stamp and text."), Name));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
