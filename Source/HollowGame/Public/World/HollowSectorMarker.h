#pragma once

#include "GameFramework/Actor.h"

#include "HollowSectorMarker.generated.h"

class UStaticMeshComponent;

/**
 * Placeholder gameplay actor that the server spawns at room sockets and connections until the real ones exist
 * (doors S2-04, bulkheads S3-01, core and extraction S3). Replicated, dormant after its first update. Visual is data
 * (BP_SectorMarker); the biome maps socket types and connection kinds to classes, so swapping needs no code.
 */
UCLASS()
class HOLLOWGAME_API AHollowSectorMarker : public AActor
{
	GENERATED_BODY()

public:
	explicit AHollowSectorMarker(const FObjectInitializer& ObjectInitializer);

	/** What the marker stands for (socket tag or connection kind); set before FinishSpawning. */
	void SetLabel(FName InLabel) { Label = InLabel; }
	FName GetLabel() const { return Label; }

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Sector")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(Replicated)
	FName Label;
};
