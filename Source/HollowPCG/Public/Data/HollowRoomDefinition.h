#pragma once

#include "Data/HollowRoomTypes.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "HollowRoomDefinition.generated.h"

class UWorld;

/**
 * One modular room of a sector (primary asset type HollowRoom). The room shell is a level streamed locally as a
 * Level Instance; gameplay actors are spawned by the server at Sockets and Connectors (ADR-0003).
 * Validated by HollowRoomRules (also on save, through Data Validation).
 */
UCLASS(BlueprintType)
class HOLLOWPCG_API UHollowRoomDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/** Static shell: geometry, light and audio only (no gameplay actors). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
	TSoftObjectPtr<UWorld> RoomLevel;

	/** Size in grid cells (UHollowLevelMetrics). The vertical slice uses one storey: Z = 1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
	FIntVector Footprint = FIntVector(1, 1, 1);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
	TArray<FHollowRoomConnector> Connectors;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
	TArray<FHollowRoomSocket> Sockets;

	/** Exactly one Hollow.Room.Type.* plus optional descriptive tags. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room", meta = (Categories = "Hollow.Room"))
	FGameplayTagContainer RoomTags;
};
