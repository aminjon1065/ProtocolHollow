#pragma once

#include "Framework/HollowGameModeBase.h"

#include "HollowRaidGameMode.generated.h"

class UHollowRaidFlowComponent;
class UHollowSectorAuthorityComponent;

/**
 * Raid game mode, thin (CLAUDE.md sec. 8): the sector lifecycle lives in UHollowSectorAuthorityComponent, phases,
 * extraction and outcomes in UHollowRaidFlowComponent (ADR-0016). Players get a pawn only once the sector is ready;
 * their loadout is asked for on arrival (ADR-0030).
 */
UCLASS()
class HOLLOWGAME_API AHollowRaidGameMode : public AHollowGameModeBase
{
	GENERATED_BODY()

public:
	explicit AHollowRaidGameMode(const FObjectInitializer& ObjectInitializer);

	UHollowSectorAuthorityComponent* GetSectorAuthority() const { return SectorAuthority; }
	UHollowRaidFlowComponent* GetRaidFlow() const { return RaidFlow; }

protected:
	/** Every arrival (login or seamless travel): the raid asks for the player's loadout (ADR-0030). */
	virtual void GenericPlayerInitialization(AController* NewPlayer) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Sector")
	TObjectPtr<UHollowSectorAuthorityComponent> SectorAuthority;

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Raid")
	TObjectPtr<UHollowRaidFlowComponent> RaidFlow;
};
