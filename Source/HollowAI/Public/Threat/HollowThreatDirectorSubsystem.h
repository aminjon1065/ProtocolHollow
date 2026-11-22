#pragma once

#include "Subsystems/WorldSubsystem.h"

#include "HollowThreatDirectorSubsystem.generated.h"

class AHollowEnemyCharacter;
class UHollowThreatConfig;

/** What the director lets the stalker do (ADR-0019). */
UENUM(BlueprintType)
enum class EHollowThreatMode : uint8
{
	/** Shadow the squad, keep out of the lamps, do not strike. */
	Stalk,
	/** Lie in wait and strike. */
	Hunt,
	/** Hide in the vents. */
	Retreat
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnHollowThreatModeChanged, AHollowEnemyCharacter* /*Stalker*/, EHollowThreatMode /*Mode*/);

/**
 * Director-lite (ADR-0019), server only: the aggression leash of the sector's one stalker. Spawns it a while after
 * the sector is up, at the vent farthest from the living players, then cycles Stalk -> Hunt -> Retreat -> Stalk:
 * a hunt starts after StalkSeconds or as soon as the hottest room nobody is in is hot enough, ends with a strike, a
 * stun or HuntSeconds; the retreat lasts RetreatSeconds. An Aether lantern's light (S6-02) sends it away from any mode. Timers only (UHollowThreatConfig::Director).
 */
UCLASS()
class HOLLOWAI_API UHollowThreatDirectorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UHollowThreatDirectorSubsystem* Get(const UObject* WorldContext);

	AHollowEnemyCharacter* GetStalker() const { return Stalker.Get(); }
	EHollowThreatMode GetMode() const { return Mode; }
	/** The stalker's mode for the stalker, Stalk for any other enemy. */
	EHollowThreatMode GetModeFor(const AActor* Enemy) const;

	/** Server: spawns the stalker now at the vent (or room) farthest from the living players. */
	AHollowEnemyCharacter* SpawnStalker();
	/** Makes Enemy the stalker, in Stalk (tests, dev). */
	void Register(AHollowEnemyCharacter* Enemy);
	void ReportStrike(const AActor* Enemy);
	void ReportStunned(const AActor* Enemy);
	/** The stalker came into an Aether lantern's light (S6-02): it retreats, an ambush broken; a retreat under way is not prolonged. */
	void ReportRepelled(const AActor* Enemy);
	void SetMode(EHollowThreatMode NewMode);

	FString Describe() const;

	/** Tuning in use (UHollowThreatConfig::Get by default); tests swap in their own before the topology is set. */
	void SetConfig(const UHollowThreatConfig* InConfig) { Config = InConfig; }

	FOnHollowThreatModeChanged OnModeChanged;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	void HandleTopologyChanged();
	void HandleSpawnTimer();
	void HandleModeTimer();
	void CheckHeat();
	bool FindSpawnLocation(FVector& OutLocation) const;

	UPROPERTY(Transient)
	TObjectPtr<const UHollowThreatConfig> Config;

	TWeakObjectPtr<AHollowEnemyCharacter> Stalker;
	EHollowThreatMode Mode = EHollowThreatMode::Stalk;
	FTimerHandle SpawnTimer;
	FTimerHandle ModeTimer;
	FTimerHandle HeatTimer;
	FDelegateHandle TopologyHandle;
};
