#pragma once

#include "GameFramework/Actor.h"

#include "HollowExtractionZone.generated.h"

class APawn;
class UBoxComponent;
class UHollowCarryableComponent;
class UStaticMeshComponent;

/**
 * The shuttle's extraction zone (ADR-0016), spawned by the server at the Extraction socket. Only a volume: the raid
 * flow asks who and what is inside with an explicit overlap query when it decides (never overlap bookkeeping), so a
 * carried core counts exactly where its body rides. No collision, no Tick.
 */
UCLASS()
class HOLLOWGAME_API AHollowExtractionZone : public AActor
{
	GENERATED_BODY()

public:
	explicit AHollowExtractionZone(const FObjectInitializer& ObjectInitializer);

	UBoxComponent* GetVolume() const { return Volume; }

	/** Pawns and carryable objects (free or carried) inside the volume now. */
	void Sample(TArray<APawn*>& OutPawns, TArray<UHollowCarryableComponent*>& OutCarryables) const;

private:
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Raid")
	TObjectPtr<UBoxComponent> Volume;

	/** Greybox landing pad (cosmetic). */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Raid")
	TObjectPtr<UStaticMeshComponent> Pad;
};
