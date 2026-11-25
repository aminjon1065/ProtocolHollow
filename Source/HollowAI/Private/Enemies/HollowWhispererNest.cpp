#include "Enemies/HollowWhispererNest.h"

#include "Components/StaticMeshComponent.h"
#include "Enemies/HollowEnemyCharacter.h"
#include "Enemies/HollowEnemySpawning.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "HollowAILog.h"
#include "Threat/HollowSwarmRules.h"
#include "Threat/HollowSwarmSubsystem.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowWhispererNest)

AHollowWhispererNest::AHollowWhispererNest(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	NetDormancy = DORM_Initial;   // it never changes: one bunch for everyone
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);   // a growth on the floor, nothing to bump into
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCanEverAffectNavigation(false);
	SetRootComponent(Mesh);
}

int32 AHollowWhispererNest::GetBroodSize() const
{
	return HollowSwarmRules::BroodSize(Seed, MinBrood, MaxBrood);
}

void AHollowWhispererNest::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		// Not now: the sector spawns its actors, then builds the navmesh the brood stands on.
		GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::TrySpawn);
	}
}

void AHollowWhispererNest::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(SpawnTimer);
	Super::EndPlay(EndPlayReason);
}

void AHollowWhispererNest::TrySpawn()
{
	if (bSpawned || SpawnBrood() > 0 || GetBroodSize() == 0)
	{
		return;
	}
	if (++Attempts < MaxSpawnAttempts)
	{
		GetWorldTimerManager().SetTimer(SpawnTimer, this, &ThisClass::TrySpawn, SpawnRetrySeconds, /*bLoop=*/false);
		return;
	}
	UE_LOG(LogHollowAI, Warning, TEXT("%s: no navmesh for its brood after %d attempts; it stays empty"), *GetName(), Attempts);
	bSpawned = true;
}

int32 AHollowWhispererNest::SpawnBrood()
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority() || bSpawned || !WhispererClass)
	{
		return 0;
	}
	const int32 Size = GetBroodSize();
	const FVector Center = GetActorLocation();
	TArray<AHollowEnemyCharacter*> Spawned;
	for (int32 Index = 0; Index < Size; ++Index)
	{
		const FVector Spot = HollowSwarmRules::BroodSpot(Seed, Index, Size, Center, SpawnRadius);
		const FRotator Facing = (Spot - Center).GetSafeNormal2D().Rotation();   // they come out facing away from the nest
		if (AHollowEnemyCharacter* Whisperer = HollowEnemySpawning::SpawnOnNavmesh(World, WhispererClass, Spot, SpawnSearchExtent, Facing))
		{
			Whisperer->SetSpawnSeed(HollowSwarmRules::MemberSeed(Seed, Index));
			Spawned.Add(Whisperer);
			Brood.Add(Whisperer);
		}
	}
	if (Spawned.IsEmpty())
	{
		return 0;   // no navmesh here yet: TrySpawn tries again
	}
	bSpawned = true;
	if (UHollowSwarmSubsystem* Swarm = UHollowSwarmSubsystem::Get(this))
	{
		Swarm->RegisterBrood(Spawned);
	}
	UE_LOG(LogHollowAI, Log, TEXT("%s (seed %d): a brood of %d of %d at %s"), *GetName(), Seed, Spawned.Num(), Size, *Center.ToCompactString());
	return Spawned.Num();
}
