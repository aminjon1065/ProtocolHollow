#pragma once

#include "Engine/DataAsset.h"

#include "HollowContractDefinition.generated.h"

/**
 * A raid contract the host picks at the hub's contracts terminal (ADR-0032), primary asset type HollowContract: what the
 * squad is sent to do and in which sector. S5-03 carries the choice into the raid's travel URL; the sector is still
 * UHollowSectorSettings::RaidSector until contracts pick it (S7-04).
 */
UCLASS(BlueprintType)
class HOLLOWGAME_API UHollowContractDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/** Whether Id names a contract of the project. */
	static bool Exists(const FPrimaryAssetId& Id);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Contract")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Contract", meta = (MultiLine = true))
	FText Description;

	/** The sector Game Feature the raid takes place in (ADR-0012). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Contract")
	FString SectorFeature;
};
