#pragma once

#include "Framework/HollowGameModeBase.h"

#include "HollowRaidGameMode.generated.h"

class UHollowSectorAuthorityComponent;

/**
 * Raid game mode, thin (CLAUDE.md sec. 8): the sector lifecycle lives in UHollowSectorAuthorityComponent, raid phases
 * and outcome follow in S3-03. Players get a pawn only once the sector is ready on every machine.
 */
UCLASS()
class HOLLOWGAME_API AHollowRaidGameMode : public AHollowGameModeBase
{
	GENERATED_BODY()

public:
	explicit AHollowRaidGameMode(const FObjectInitializer& ObjectInitializer);

	UHollowSectorAuthorityComponent* GetSectorAuthority() const { return SectorAuthority; }

protected:
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Sector")
	TObjectPtr<UHollowSectorAuthorityComponent> SectorAuthority;
};
