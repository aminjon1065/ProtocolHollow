#include "Framework/HollowGameState.h"

#include "Assembly/HollowSectorStateComponent.h"
#include "Raid/HollowRaidStateComponent.h"
#include "Resonance/HollowSquadResonanceComponent.h"
#include "Sector/HollowSectorLoaderComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowGameState)

AHollowGameState::AHollowGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SectorState = CreateDefaultSubobject<UHollowSectorStateComponent>(TEXT("SectorState"));
	SectorLoader = CreateDefaultSubobject<UHollowSectorLoaderComponent>(TEXT("SectorLoader"));
	RaidState = CreateDefaultSubobject<UHollowRaidStateComponent>(TEXT("RaidState"));
	SquadResonance = CreateDefaultSubobject<UHollowSquadResonanceComponent>(TEXT("SquadResonance"));
}

void AHollowGameState::AddPlayerState(APlayerState* PlayerState)
{
	Super::AddPlayerState(PlayerState);
	if (SquadResonance)
	{
		SquadResonance->MirrorTierTo(PlayerState);
	}
}
