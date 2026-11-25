#include "World/HollowEnemyDropSubsystem.h"

#include "Enemies/HollowEnemyCharacter.h"
#include "Enemies/HollowEnemyDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "HollowGameLog.h"
#include "Items/HollowItemDefinition.h"
#include "World/HollowLootPickup.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowEnemyDropSubsystem)

UHollowEnemyDropSubsystem* UHollowEnemyDropSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UHollowEnemyDropSubsystem>() : nullptr;
}

bool UHollowEnemyDropSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UHollowEnemyDropSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	DiedHandle = AHollowEnemyCharacter::OnAnyEnemyDied().AddUObject(this, &ThisClass::HandleEnemyDied);
}

void UHollowEnemyDropSubsystem::Deinitialize()
{
	AHollowEnemyCharacter::OnAnyEnemyDied().Remove(DiedHandle);
	Super::Deinitialize();
}

void UHollowEnemyDropSubsystem::HandleEnemyDied(AHollowEnemyCharacter& Enemy)
{
	UWorld* World = GetWorld();
	const UHollowEnemyDefinition* Definition = Enemy.GetDefinition();
	if (Enemy.GetWorld() != World || !World || !Enemy.HasAuthority() || !Definition || !Definition->Drop.Rolls(Enemy.GetSpawnSeed())
		|| !UAssetManager::IsInitialized())
	{
		return;
	}
	const FHollowEnemyDrop& Drop = Definition->Drop;
	UHollowItemDefinition* Item = Cast<UHollowItemDefinition>(UAssetManager::Get().GetPrimaryAssetPath(Drop.Item).TryLoad());
	const TSubclassOf<AHollowLootPickup> PickupClass = TSubclassOf<AHollowLootPickup>(Drop.PickupClass.LoadSynchronous());
	if (!Item || !PickupClass)
	{
		UE_LOG(LogHollowGame, Warning, TEXT("%s: drop %s (%s) cannot spawn"), *Enemy.GetName(), *Drop.Item.ToString(), *Drop.PickupClass.ToString());
		return;
	}
	const FTransform Transform(Enemy.GetActorRotation(), Enemy.GetActorLocation());
	AHollowLootPickup* Pickup = World->SpawnActorDeferred<AHollowLootPickup>(PickupClass, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Pickup)
	{
		return;
	}
	Pickup->SetLoot(Item, Drop.Count);
	Pickup->FinishSpawning(Transform);
	Pickup->Launch(FVector::UpVector * Drop.PopSpeed);
	++Drops;
	LastDrop = Pickup;
	UE_LOG(LogHollowGame, Log, TEXT("%s leaves %d x %s"), *Enemy.GetName(), Drop.Count, *Drop.Item.ToString());
}
