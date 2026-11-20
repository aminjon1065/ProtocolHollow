#pragma once

#include "Noise/HollowNoiseTypes.h"
#include "Subsystems/WorldSubsystem.h"

#include "HollowActivityHeatmapSubsystem.generated.h"

class APawn;
class UHollowThreatConfig;

/**
 * Where the squad spends its time (ADR-0019), server only: heat per room of the sector topology from living players'
 * presence and noise, and per portal from their crossings, decaying with a half-life (UHollowThreatConfig). The
 * Echo-Stalker lies in wait where it is hottest; timers and the noise delegate drive it (no Tick).
 */
UCLASS()
class HOLLOWAI_API UHollowActivityHeatmapSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UHollowActivityHeatmapSubsystem* Get(const UObject* WorldContext);

	float GetRoomHeat(int32 Room) const;
	float GetPortalHeat(int32 Portal) const;
	/** Hottest room with any heat (bWithoutPlayers: none of the living players is in it now); INDEX_NONE if none. */
	int32 HottestRoom(bool bWithoutPlayers) const;
	/** Hottest portal of a room with any heat, INDEX_NONE if none. */
	int32 HottestPortalOf(int32 Room) const;
	/** Room holding the most living players (ties: the lower index), INDEX_NONE if nobody is in a room. */
	int32 SquadRoom() const;
	bool IsOccupied(int32 Room) const;

	/** One sampling step: decay, presence, crossings (the timer's; tests step it themselves). */
	void Sample(float DeltaSeconds);

	/** Heat from elsewhere (scripted events, tests): it decays like the rest. */
	void AddRoomHeat(int32 Room, float Heat);
	void AddPortalHeat(int32 Portal, float Heat);

	/** Rooms by heat for Hollow.AI.Heat. */
	FString Describe() const;

	/** Tuning in use (UHollowThreatConfig::Get by default); tests swap in their own before the topology is set. */
	void SetConfig(const UHollowThreatConfig* InConfig) { Config = InConfig; }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	void HandleTopologyChanged();
	void HandleNoise(const FHollowNoiseReport& Report);
	void HandleSampleTimer();
	TArray<int32> PlayerRooms() const;

	UPROPERTY(Transient)
	TObjectPtr<const UHollowThreatConfig> Config;

	TArray<float> RoomHeat;
	TArray<float> PortalHeat;
	/** Each player's room at the last sample, for crossings. */
	TArray<TPair<TWeakObjectPtr<APawn>, int32>> LastRooms;

	FTimerHandle SampleTimer;
	FDelegateHandle TopologyHandle;
	FDelegateHandle NoiseHandle;
};
