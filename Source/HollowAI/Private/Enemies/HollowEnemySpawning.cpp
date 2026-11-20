#include "Enemies/HollowEnemySpawning.h"

#include "Components/CapsuleComponent.h"
#include "Enemies/HollowEnemyCharacter.h"
#include "Engine/World.h"
#include "HollowAILog.h"
#include "NavigationSystem.h"

AHollowEnemyCharacter* HollowEnemySpawning::SpawnOnNavmesh(UWorld* World, const TSubclassOf<AHollowEnemyCharacter> Class, const FVector& Wanted,
	const float SearchExtent, const FRotator& Facing)
{
	if (!World || !Class)
	{
		return nullptr;
	}
	// The class defaults have not sized their nav agent from the capsule yet: do it here.
	const AHollowEnemyCharacter* Defaults = Class->GetDefaultObject<AHollowEnemyCharacter>();
	FNavAgentProperties Agent = Defaults->GetNavAgentPropertiesRef();
	Agent.AgentRadius = Defaults->GetCapsuleComponent()->GetScaledCapsuleRadius();
	Agent.AgentHeight = Defaults->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2.f;
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	const ANavigationData* NavData = Navigation ? Navigation->GetNavDataForProps(Agent, Wanted) : nullptr;
	FNavLocation OnNavmesh;
	if (!NavData || !Navigation->ProjectPointToNavigation(Wanted, OnNavmesh, FVector(SearchExtent), NavData))
	{
		UE_LOG(LogHollowAI, Warning, TEXT("No navmesh for %s near %s."), *Class->GetName(), *Wanted.ToCompactString());
		return nullptr;
	}
	const FVector Location = OnNavmesh.Location + FVector(0.0, 0.0, Defaults->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	return World->SpawnActor<AHollowEnemyCharacter>(Class, Location, Facing, Params);
}
