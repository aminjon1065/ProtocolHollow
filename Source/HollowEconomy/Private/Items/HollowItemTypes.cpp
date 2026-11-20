#include "Items/HollowItemTypes.h"

#include "Engine/AssetManager.h"
#include "HollowEconomyLog.h"
#include "Items/HollowItemDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowItemTypes)

void FHollowItemCatalog::Add(const FPrimaryAssetId& Item, const int32 Value, const bool bInsurable)
{
	FEntry& Entry = Entries.FindOrAdd(Item);
	Entry.Value = Value;
	Entry.bInsurable = bInsurable;
}

int32 FHollowItemCatalog::GetValue(const FPrimaryAssetId& Item) const
{
	const FEntry* Entry = Entries.Find(Item);
	return Entry ? Entry->Value : 0;
}

bool FHollowItemCatalog::IsInsurable(const FPrimaryAssetId& Item) const
{
	const FEntry* Entry = Entries.Find(Item);
	return Entry && Entry->bInsurable;
}

FHollowItemCatalog FHollowItemCatalog::Load(const TConstArrayView<FPrimaryAssetId> Items)
{
	FHollowItemCatalog Catalog;
	if (!UAssetManager::IsInitialized())
	{
		return Catalog;
	}
	for (const FPrimaryAssetId& Id : Items)
	{
		if (Catalog.Contains(Id))
		{
			continue;
		}
		const UHollowItemDefinition* Definition = Cast<UHollowItemDefinition>(UAssetManager::Get().GetPrimaryAssetPath(Id).TryLoad());
		if (!Definition)
		{
			UE_LOG(LogHollowEconomy, Warning, TEXT("Unknown item %s: worth nothing, never insured."), *Id.ToString());
			continue;
		}
		Catalog.Add(Id, Definition->BaseValue, Definition->bInsurable);
	}
	return Catalog;
}
