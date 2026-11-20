#pragma once

#include "Engine/DataAsset.h"

#include "HollowBreachableDefinition.generated.h"

class UGeometryCollection;
class UNavArea;
class UPhysicalMaterial;

/** One breach zone of a bulkhead: a panel that blocks until its health runs out (Docs/Architecture/03 sec. 3.4). */
USTRUCT(BlueprintType)
struct FHollowBreachZone
{
	GENERATED_BODY()

	/** Panel box in actor space: X across the opening, Y along the wall, Z up from the floor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone")
	FVector Center = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone", meta = (Units = "Centimeters"))
	FVector HalfSize = FVector::ZeroVector;

	/** Structural damage the panel takes before it breaks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone", meta = (ClampMin = "0"))
	float MaxHealth = 0.f;

	/** The passage opens once every zone with this flag is broken. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone")
	bool bRequiredForPassage = true;

	/** Cosmetic pieces of the panel, pivot at the panel centre; loaded only on machines that render. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone")
	TSoftObjectPtr<UGeometryCollection> Fracture;
};

/**
 * Tuning of a breachable bulkhead variant, primary asset type HollowBreachable (Docs/Architecture/03 sec. 3.4).
 * Gameplay (zones, health, passage) is the same on every machine; the presentation part only shapes cosmetics.
 */
UCLASS(BlueprintType)
class HOLLOWPHYSICS_API UHollowBreachableDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;

	explicit UHollowBreachableDefinition(const FObjectInitializer& ObjectInitializer);

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/** Sum of the zones' MaxHealth. */
	float GetTotalHealth() const;

	/** Bits of the zones that must break before the passage opens. */
	uint8 GetPassageMask() const;

	/** At most FHollowBreachNetState::MaxZones. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zones")
	TArray<FHollowBreachZone> Zones;

	/** A break younger than this is played live where this machine's budget allows; older ones snap (late join). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (ClampMin = "0", Units = "Seconds"))
	float FreshWindowSeconds = 0.f;

	/** Launch speed of a zone's debris along the impact direction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float DebrisSpeed = 0.f;

	/** Sleep thresholds and friction of live debris. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	TSoftObjectPtr<UPhysicalMaterial> DebrisMaterial;

	/** Height and depth of the static rubble left under a broken zone (snap, and after live debris is gone). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (ClampMin = "0", Units = "Centimeters"))
	float RubbleHeight = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (ClampMin = "0", Units = "Centimeters"))
	float RubbleDepth = 0.f;

	/** Nav link area while the passage is shut (Docs/Architecture/03 sec. 3.7): Standard agents excluded, Heavy breaks through. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	TSubclassOf<UNavArea> AreaIntact;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	TSubclassOf<UNavArea> AreaBreached;

	/** The nav blocker goes this long after the breach (the link already carries agents): a low-priority tile rebuild. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation", meta = (ClampMin = "0", Units = "Seconds"))
	float BlockerRemovalDelay = 0.f;
};
