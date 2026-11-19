#pragma once

#include "Subsystems/EngineSubsystem.h"
#include "UObject/PrimaryAssetId.h"

#include "HollowSectorFeatureSubsystem.generated.h"

/**
 * Lifecycle of sector Game Feature plugins (ADR-0012). Every sector plugin is Registered at startup (content mounted,
 * room ids known to the asset manager) and Active only while a raid uses it. Activation is local to each machine:
 * the host picks the sector, clients learn it with the sector manifest (S2-02) and activate it themselves.
 * Engine subsystem with reference counts: several game instances in one process (PIE co-op) share one plugin state.
 */
UCLASS()
class HOLLOWGAME_API UHollowSectorFeatureSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	DECLARE_DELEGATE_OneParam(FOnHollowSectorChanged, bool /*bSuccess*/);

	/** Null only before the engine is initialized. */
	static UHollowSectorFeatureSubsystem* Get();

	/** Loads and activates the sector plugin (or adds a reference if it is already in use). */
	void ActivateSector(const FString& PluginName, FOnHollowSectorChanged OnDone);

	/** Releases one reference; the last one returns the plugin to Registered. */
	void DeactivateSector(const FString& PluginName, FOnHollowSectorChanged OnDone);

	bool IsSectorActive(const FString& PluginName) const;

	/** One-line state for logs and the console: plugin state, references, rooms. */
	FString DescribeSector(const FString& PluginName) const;

	/** Room definitions the asset manager knows inside the plugin, sorted by PrimaryAssetId (deterministic order). */
	static TArray<FPrimaryAssetId> GetSectorRooms(const FString& PluginName);

	/** Biome definition (generator rules) of the sector plugin; invalid if it has none. The first by id if several. */
	static FPrimaryAssetId GetSectorBiome(const FString& PluginName);

private:
	bool ResolveURL(const FString& PluginName, FString& OutURL) const;

	/** Primary assets of a type inside the plugin's mount point, sorted by id. */
	static TArray<FPrimaryAssetId> GetSectorAssets(const FString& PluginName, const FPrimaryAssetType& Type);

	/** Active users per plugin name. */
	TMap<FString, int32> References;
};
