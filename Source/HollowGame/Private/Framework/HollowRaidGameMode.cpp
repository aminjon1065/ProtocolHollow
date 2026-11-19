#include "Framework/HollowRaidGameMode.h"

#include "Sector/HollowSectorAuthorityComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowRaidGameMode)

AHollowRaidGameMode::AHollowRaidGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SessionPhase = EHollowSessionPhase::Raid;   // drop-in joins are refused during a raid (ADR-0006)
	SectorAuthority = CreateDefaultSubobject<UHollowSectorAuthorityComponent>(TEXT("SectorAuthority"));
}

void AHollowRaidGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// Before the sector is ready there is no floor to stand on: the authority restarts everyone when it is.
	if (SectorAuthority->IsSectorReady())
	{
		Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	}
}

void AHollowRaidGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
	SectorAuthority->HandlePlayerLeft(Exiting);
}
