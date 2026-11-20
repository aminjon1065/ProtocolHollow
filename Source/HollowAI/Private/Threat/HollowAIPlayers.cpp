#include "Threat/HollowAIPlayers.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagAssetInterface.h"
#include "Tags/HollowGameplayTags.h"

bool HollowAIPlayers::IsLivingPlayer(const AActor* Actor)
{
	const APawn* Pawn = Cast<APawn>(Actor);
	if (!Pawn || !Pawn->IsPlayerControlled())
	{
		return false;
	}
	const IGameplayTagAssetInterface* Tags = Cast<IGameplayTagAssetInterface>(Pawn);
	return !Tags || !Tags->HasMatchingGameplayTag(HollowTags::State_Dead);
}

void HollowAIPlayers::ForEach(const UWorld* World, const TFunctionRef<void(APawn&)> Visit)
{
	if (!World)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APawn* Pawn = It->Get() ? It->Get()->GetPawn() : nullptr;
		if (IsLivingPlayer(Pawn))
		{
			Visit(*Pawn);
		}
	}
}
