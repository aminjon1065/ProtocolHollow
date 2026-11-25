#pragma once

#include "Engine/DataAsset.h"

#include "HollowLoreEntryDefinition.generated.h"

/**
 * One crew log of the Ark Hollow (NAR-01b, ADR-0050), primary asset type HollowLore: a datapad in its sector holds it,
 * the hub's Codex shows it once found. All twelve live in the base game (/Game/Hollow/Story/Lore, create_story_assets.py),
 * not in the sector plugins: the hub reads them without a sector loaded. Its texts are ST_Lore entries (ADR-0048).
 */
UCLASS(BlueprintType)
class HOLLOWGAME_API UHollowLoreEntryDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/** Every crew log of the project, loaded (synchronously: a dozen small assets), by Number. */
	static TArray<const UHollowLoreEntryDefinition*> LoadAll();

	/** The crew log Id names, loaded on demand; null for anything else. */
	static const UHollowLoreEntryDefinition* Load(const FPrimaryAssetId& Id);

	/** The sector Game Feature its datapad lies in (the contracts' SectorFeature, e.g. HollowSector_Docks). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lore")
	FString SectorFeature;

	/** The sector as the Codex names it ("Orbital Docks"; one ST_World entry shared by the sector's logs). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lore")
	FText SectorName;

	/** Its place in the canon (1..12, unique): the Codex lists the logs by it, a sector's together. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lore", meta = (ClampMin = "1"))
	int32 Number = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lore")
	FText Title;

	/** Who wrote it ("Teodor Lisk, dock master"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lore")
	FText Author;

	/** The log's stamp ("Ship's log, day 0"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lore")
	FText Stamp;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lore", meta = (MultiLine = true))
	FText Body;
};
