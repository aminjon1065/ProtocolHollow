#pragma once

#include "GameFramework/GameStateBase.h"

#include "HollowGameState.generated.h"

class UHollowSectorLoaderComponent;
class UHollowSectorStateComponent;

/** Thin game state (CLAUDE.md sec. 8): state lives in its components (Docs/Architecture/02 sec. 2.2). */
UCLASS()
class HOLLOWGAME_API AHollowGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	explicit AHollowGameState(const FObjectInitializer& ObjectInitializer);

	UHollowSectorStateComponent* GetSectorState() const { return SectorState; }

private:
	/** Replicated manifest and readiness (PCG). */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Sector")
	TObjectPtr<UHollowSectorStateComponent> SectorState;

	/** Assembles the sector on this machine and reports it (every machine). */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Sector")
	TObjectPtr<UHollowSectorLoaderComponent> SectorLoader;
};
