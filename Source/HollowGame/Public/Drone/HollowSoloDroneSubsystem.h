#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "Templates/SubclassOf.h"

#include "HollowSoloDroneSubsystem.generated.h"

class AHollowExoFrameCharacter;
class AHollowSoloDrone;
class UHollowRaidStateComponent;
struct FHollowRaidNetState;

/**
 * Deploys the solo drone (DRONE-01, ADR-0054). Server only: when the raid starts (InRaid: the pawns are spawned after
 * Deploying begins) and exactly one player is in it, the drone (UHollowRaidSettings::SoloDroneClass) is spawned for that
 * player; never in co-op, never in the hub (no raid state there). Once a raid: players only join in the hub, so the
 * count cannot grow, and a co-op squad shrinking to one does not get a drone halfway.
 */
UCLASS()
class HOLLOWGAME_API UHollowSoloDroneSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UHollowSoloDroneSubsystem* Get(const UObject* WorldContext);

	/** A raid of this many players is solo, so it gets a drone (exactly one: solo and co-op run the same code otherwise). */
	static bool IsSoloRaid(int32 Players) { return Players == 1; }

	/**
	 * Server: spawns DroneClass for the only one of Players if the raid is solo (IsSoloRaid) and no drone is out yet;
	 * null otherwise (co-op, nobody, no class).
	 */
	AHollowSoloDrone* Deploy(const TArray<AHollowExoFrameCharacter*>& Players, TSubclassOf<AHollowSoloDrone> DroneClass);

	AHollowSoloDrone* GetDrone() const { return Drone.Get(); }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

private:
	void HandleRaidState(const FHollowRaidNetState& OldState, const FHollowRaidNetState& NewState);
	/** The living players' characters (server). */
	TArray<AHollowExoFrameCharacter*> RaidPlayers() const;

	TWeakObjectPtr<AHollowSoloDrone> Drone;
	TWeakObjectPtr<UHollowRaidStateComponent> RaidState;
	FDelegateHandle StateHandle;
	/** The raid's player count was looked at (once a raid). */
	bool bDecided = false;
};
