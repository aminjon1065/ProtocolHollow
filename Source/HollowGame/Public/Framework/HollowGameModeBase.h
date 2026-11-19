#pragma once

#include "GameFramework/GameModeBase.h"
#include "Session/HollowSessionTypes.h"

#include "HollowGameModeBase.generated.h"

/**
 * Base game mode of Protocol: Hollow. Kept thin (CLAUDE.md sec. 8): rules live in components and subsystems.
 * Gates incoming players with HollowSessionRules::ValidateJoin (hub-only joins in the vertical slice, ADR-0006).
 */
UCLASS()
class HOLLOWGAME_API AHollowGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;

	EHollowSessionPhase GetSessionPhase() const { return SessionPhase; }

protected:
	/** Phase this game mode represents; raid game modes set Raid so drop-in joins are refused. */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Session")
	EHollowSessionPhase SessionPhase = EHollowSessionPhase::Hub;
};
