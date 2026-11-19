#pragma once

#include "Assembly/HollowSectorManifest.h"
#include "Components/GameStateComponent.h"

#include "HollowSectorStateComponent.generated.h"

/**
 * Replicated sector state on the GameState (ADR-0003): the manifest, pushed once per assembly, and whether every
 * player has the sector ready. Written by the server only; every machine reacts through the delegates.
 */
UCLASS(ClassGroup = (Hollow))
class HOLLOWPCG_API UHollowSectorStateComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnHollowManifestChanged, const FHollowSectorManifest&);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnHollowSectorReadyChanged, bool /*bReady*/);

	explicit UHollowSectorStateComponent(const FObjectInitializer& ObjectInitializer);

	/** The component on the world's GameState, if any. */
	static UHollowSectorStateComponent* Get(const UWorld* World);

	/** Server only. */
	void SetManifest(const FHollowSectorManifest& NewManifest);

	/** Server only. */
	void SetSectorReady(bool bReady);

	const FHollowSectorManifest& GetManifest() const { return Manifest; }
	bool IsSectorReady() const { return bSectorReady; }

	FOnHollowManifestChanged OnManifestChanged;
	FOnHollowSectorReadyChanged OnSectorReadyChanged;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_Manifest();

	UFUNCTION()
	void OnRep_SectorReady();

	UPROPERTY(ReplicatedUsing = OnRep_Manifest)
	FHollowSectorManifest Manifest;

	UPROPERTY(ReplicatedUsing = OnRep_SectorReady)
	bool bSectorReady = false;
};
