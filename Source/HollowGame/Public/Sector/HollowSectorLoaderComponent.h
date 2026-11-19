#pragma once

#include "Components/GameStateComponent.h"

#include "HollowSectorLoaderComponent.generated.h"

struct FHollowSectorManifest;

/**
 * Brings the replicated sector to life on this machine (every machine, server included): activates the sector Game
 * Feature, assembles the manifest and reports BuildId + local LayoutHash to the server through the local player
 * controller (ADR-0003, ADR-0012). The host reports like any client, so solo and co-op share one path.
 */
UCLASS(ClassGroup = (Hollow))
class HOLLOWGAME_API UHollowSectorLoaderComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	explicit UHollowSectorLoaderComponent(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleManifest(const FHollowSectorManifest& Manifest);
	void HandleSectorActive(int32 ForBuildId, bool bActive);
	void HandleAssembled(int32 BuildId, uint32 LocalHash);
	void TryReport();
	void ReleaseSector();

	FString ActiveSector;
	int32 RequestedBuildId = 0;
	TOptional<TPair<int32, uint32>> PendingReport;
	FTimerHandle RetryTimer;
	FDelegateHandle ManifestHandle;
	FDelegateHandle AssembledHandle;
};
