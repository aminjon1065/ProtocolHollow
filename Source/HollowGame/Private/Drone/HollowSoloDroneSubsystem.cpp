#include "Drone/HollowSoloDroneSubsystem.h"

#include "Character/HollowExoFrameCharacter.h"
#include "Drone/HollowSoloDrone.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HollowGameLog.h"
#include "Raid/HollowRaidSettings.h"
#include "Raid/HollowRaidStateComponent.h"
#include "Raid/HollowRaidStateTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowSoloDroneSubsystem)

UHollowSoloDroneSubsystem* UHollowSoloDroneSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UHollowSoloDroneSubsystem>() : nullptr;
}

bool UHollowSoloDroneSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UHollowSoloDroneSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	UHollowRaidStateComponent* State = InWorld.GetNetMode() != NM_Client ? UHollowRaidStateComponent::Get(&InWorld) : nullptr;
	if (State)
	{
		RaidState = State;
		StateHandle = State->OnStateChanged.AddUObject(this, &ThisClass::HandleRaidState);
		HandleRaidState(State->GetState(), State->GetState());   // a raid already under way (a late subsystem)
	}
}

void UHollowSoloDroneSubsystem::Deinitialize()
{
	if (UHollowRaidStateComponent* State = RaidState.Get())
	{
		State->OnStateChanged.Remove(StateHandle);
	}
	Super::Deinitialize();
}

void UHollowSoloDroneSubsystem::HandleRaidState(const FHollowRaidNetState& /*OldState*/, const FHollowRaidNetState& NewState)
{
	if (bDecided || NewState.Phase != EHollowRaidPhase::InRaid)   // the pawns are down only after Deploying began
	{
		return;
	}
	bDecided = true;
	const TArray<AHollowExoFrameCharacter*> Players = RaidPlayers();
	if (!IsSoloRaid(Players.Num()))
	{
		UE_LOG(LogHollowGame, Log, TEXT("Solo drone: none, %d players in the raid"), Players.Num());
		return;
	}
	const TSoftClassPtr<AHollowSoloDrone>& DroneClass = GetDefault<UHollowRaidSettings>()->SoloDroneClass;
	const TSubclassOf<AHollowSoloDrone> Loaded = DroneClass.LoadSynchronous();   // once a raid, while the squad lands
	UE_CLOG(!Loaded, LogHollowGame, Error, TEXT("Solo drone: its class %s does not load"), *DroneClass.ToString());
	Deploy(Players, Loaded);
}

TArray<AHollowExoFrameCharacter*> UHollowSoloDroneSubsystem::RaidPlayers() const
{
	TArray<AHollowExoFrameCharacter*> Players;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AHollowExoFrameCharacter* Character = It->Get() ? Cast<AHollowExoFrameCharacter>(It->Get()->GetPawn()) : nullptr;
		if (Character && !Character->IsDead())
		{
			Players.Add(Character);
		}
	}
	return Players;
}

AHollowSoloDrone* UHollowSoloDroneSubsystem::Deploy(const TArray<AHollowExoFrameCharacter*>& Players, const TSubclassOf<AHollowSoloDrone> DroneClass)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || !DroneClass || Drone.IsValid() || !IsSoloRaid(Players.Num()) || !IsValid(Players[0]))
	{
		return nullptr;
	}
	AHollowExoFrameCharacter* Player = Players[0];
	AHollowSoloDrone* NewDrone = World->SpawnActorDeferred<AHollowSoloDrone>(DroneClass, Player->GetActorTransform(), Player, Player,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!NewDrone)
	{
		return nullptr;
	}
	NewDrone->SetPlayer(Player);
	NewDrone->FinishSpawning(Player->GetActorTransform());
	Drone = NewDrone;
	UE_LOG(LogHollowGame, Log, TEXT("Solo drone: %s deployed for %s"), *NewDrone->GetName(), *Player->GetName());
	return NewDrone;
}
