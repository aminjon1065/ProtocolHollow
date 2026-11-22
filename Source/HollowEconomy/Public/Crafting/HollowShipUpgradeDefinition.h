#pragma once

#include "Engine/DataAsset.h"
#include "Items/HollowItemTypes.h"

#include "HollowShipUpgradeDefinition.generated.h"

struct FHollowUpgradeTerms;

/**
 * A permanent upgrade of the shuttle Skiff (ADR-0032), primary asset type HollowUpgrade: bought once with credits at the
 * hub's workbench, kept in the profile for good. Its effects are data; one upgrade may have several.
 */
UCLASS(BlueprintType)
class HOLLOWECONOMY_API UHollowShipUpgradeDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/** The plain terms the rules work with (HollowCraftingRules). */
	FHollowUpgradeTerms ToTerms() const;

	/** Every upgrade of the project, in PrimaryAssetId order (loaded synchronously: tiny data assets). */
	static TArray<FHollowUpgradeTerms> LoadAll();

	/** The same by id. */
	static TMap<FPrimaryAssetId, FHollowUpgradeTerms> LoadAllById();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade", meta = (MultiLine = true))
	FText Description;

	/** Credits, once. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade", meta = (ClampMin = "1"))
	int32 Price = 1;

	/** More loadout slots may be insured at once. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects", meta = (ClampMin = "0"))
	int32 ExtraInsuredSlots = 0;

	/** Extra percent of their value sold Aether cores bring on extraction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects", meta = (ClampMin = "0", Units = "Percent"))
	int32 CoreValuePercent = 0;

	/** Given to the player's raid inventory every raid from the ship's stores: used first, never taken from the profile. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects", meta = (TitleProperty = "Item"))
	TArray<FHollowItemStack> RaidItems;

	/** Equip slots it adds to the loadout's limits (S6-04: the Reliquary, a second relic). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects", meta = (TitleProperty = "Slot"))
	TArray<FHollowEquipSlotLimit> ExtraEquipSlots;
};
