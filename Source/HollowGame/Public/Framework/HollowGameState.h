#pragma once

#include "GameFramework/GameStateBase.h"

#include "HollowGameState.generated.h"

class UHollowRaidStateComponent;
class UHollowSectorLoaderComponent;
class UHollowSectorStateComponent;
class UHollowSquadResonanceComponent;

/** Thin game state (CLAUDE.md sec. 8): state lives in its components (Docs/Architecture/02 sec. 2.2). */
UCLASS()
class HOLLOWGAME_API AHollowGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	explicit AHollowGameState(const FObjectInitializer& ObjectInitializer);

	UHollowSectorStateComponent* GetSectorState() const { return SectorState; }
	UHollowRaidStateComponent* GetRaidState() const { return RaidState; }
	UHollowSquadResonanceComponent* GetSquadResonance() const { return SquadResonance; }

private:
	/** Replicated manifest and readiness (PCG). */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Sector")
	TObjectPtr<UHollowSectorStateComponent> SectorState;

	/** Assembles the sector on this machine and reports it (every machine). */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Sector")
	TObjectPtr<UHollowSectorLoaderComponent> SectorLoader;

	/** Replicated raid phase and extraction (S3-03; idle in the hub). */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Raid")
	TObjectPtr<UHollowRaidStateComponent> RaidState;

	/** Replicated squad Resonance (S4-02, GAS; 0 in the hub). */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Resonance")
	TObjectPtr<UHollowSquadResonanceComponent> SquadResonance;
};
