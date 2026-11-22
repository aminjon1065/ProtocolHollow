#include "Framework/HollowRaidGameMode.h"

#include "Framework/HollowPlayerController.h"
#include "Raid/HollowRaidFlowComponent.h"
#include "Raid/HollowRaidLauncher.h"
#include "Sector/HollowSectorAuthorityComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowRaidGameMode)

AHollowRaidGameMode::AHollowRaidGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SessionPhase = EHollowSessionPhase::Raid;   // drop-in joins are refused during a raid (ADR-0006)
	SectorAuthority = CreateDefaultSubobject<UHollowSectorAuthorityComponent>(TEXT("SectorAuthority"));
	RaidFlow = CreateDefaultSubobject<UHollowRaidFlowComponent>(TEXT("RaidFlow"));
}

void AHollowRaidGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	// Every raid has a contract (the default one without the hub's choice), and the contract picks the sector (ADR-0041).
	const FPrimaryAssetId Contract = HollowRaidLauncher::ContractFromOptions(Options);
	RaidFlow->SetContract(Contract);
	SectorAuthority->SetContract(Contract);
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
