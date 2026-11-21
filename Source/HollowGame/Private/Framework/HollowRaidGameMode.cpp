#include "Framework/HollowRaidGameMode.h"

#include "Framework/HollowPlayerController.h"
#include "Raid/HollowRaidFlowComponent.h"
#include "Sector/HollowSectorAuthorityComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowRaidGameMode)

AHollowRaidGameMode::AHollowRaidGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SessionPhase = EHollowSessionPhase::Raid;   // drop-in joins are refused during a raid (ADR-0006)
	SectorAuthority = CreateDefaultSubobject<UHollowSectorAuthorityComponent>(TEXT("SectorAuthority"));
	RaidFlow = CreateDefaultSubobject<UHollowRaidFlowComponent>(TEXT("RaidFlow"));
}

void AHollowRaidGameMode::GenericPlayerInitialization(AController* NewPlayer)
{
	Super::GenericPlayerInitialization(NewPlayer);
	RaidFlow->HandlePlayerArrived(Cast<AHollowPlayerController>(NewPlayer));
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
	RaidFlow->HandlePlayerLeft(Exiting);
	Super::Logout(Exiting);
	SectorAuthority->HandlePlayerLeft(Exiting);
}
