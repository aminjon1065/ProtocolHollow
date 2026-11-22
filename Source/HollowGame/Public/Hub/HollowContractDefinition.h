#pragma once

#include "Engine/DataAsset.h"

#include "HollowContractDefinition.generated.h"

/**
 * A raid contract the host picks at the hub's contracts terminal (ADR-0032), primary asset type HollowContract: what the
 * squad is sent to do, in which sector and for what. The choice travels in the raid's URL and picks the sector Game
 * Feature the raid assembles (ADR-0041); the hub shows its sector and reward to every player.
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

	/** The contract Id names, loaded on demand (a few texts and ids: no hitch); null for anything else. */
	static const UHollowContractDefinition* Load(const FPrimaryAssetId& Id);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Contract")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Contract", meta = (MultiLine = true))
	FText Description;

	/** The sector Game Feature the raid takes place in (ADR-0012): the raid assembles it (ADR-0041). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Contract")
	FString SectorFeature;

	/** The sector's name on the contracts screen ("Sector II - Reactor Cathedral"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Contract")
	FText SectorName;

	/** What the contract pays: the items the squad brings home, priced on the screen as their extraction pays them. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Contract", meta = (AllowedTypes = "HollowItem"))
	TArray<FPrimaryAssetId> RewardItems;
};
