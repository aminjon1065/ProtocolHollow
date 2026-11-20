#pragma once

// Shared navigable test scene of the enemy tests (Iron Pilgrim S3-04, Echo-Stalker and pulse mine S4-01).
#include "Tests/HollowGameTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "AI/AISystemBase.h"
#include "AI/NavigationSystemBase.h"
#include "Attributes/HollowHealthSet.h"
#include "Brain/HollowAIController.h"
#include "Components/StateTreeAIComponent.h"
#include "Damage/HollowDamageMath.h"
#include "Damage/HollowDamageSettings.h"
#include "Doors/HollowDoor.h"
#include "Doors/HollowDoorDefinition.h"
#include "Doors/HollowDoorStateComponent.h"
#include "Enemies/HollowEnemyCharacter.h"
#include "Enemies/HollowEnemyDefinition.h"
#include "Abilities/HollowEnemyAttackDefinition.h"
#include "Abilities/HollowGA_PilgrimAttacks.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "GameplayEffect.h"
#include "NavAreas/NavArea.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "StateTree.h"
#include "Tags/HollowGameplayTags.h"
#include "UObject/Package.h"

namespace HollowEnemyTests
{
	// Content under test (create_pilgrim_assets.py, create_door_assets.py, create_gas_assets.py).
	const TCHAR* const PilgrimClass = TEXT("/Game/Hollow/Characters/Enemies/IronPilgrim/BP_IronPilgrim.BP_IronPilgrim_C");
	const TCHAR* const MeleeAttack = TEXT("/Game/Hollow/Characters/Enemies/IronPilgrim/DA_Attack_Pilgrim_Melee.DA_Attack_Pilgrim_Melee");
	const TCHAR* const DoorClass = TEXT("/Game/Hollow/World/Kit/Doors/BP_Door.BP_Door_C");
	const TCHAR* const InitEffectClass = TEXT("/Game/Hollow/GAS/Effects/GE_ExoFrame_InitAttributes.GE_ExoFrame_InitAttributes_C");
	const TCHAR* const WindupCueClass = TEXT("/Game/Hollow/GAS/Cues/GCN_Enemy_Windup.GCN_Enemy_Windup_C");
	const TCHAR* const WelderAbilityClass = TEXT("/Game/Hollow/Tools/Welder/GA_Tool_Welder.GA_Tool_Welder_C");
	const TCHAR* const BeamCueClass = TEXT("/Game/Hollow/GAS/Cues/GCN_Tool_Beam.GCN_Tool_Beam_C");
	const TCHAR* const CubeMesh = TEXT("/Engine/BasicShapes/Cube.Cube");

	// Test scene (uu), the S2-05 nav test layout: a floor, a wall across it at WallX with a door in the middle, and a
	// long detour round the south end, so a Heavy agent prefers breaking a fully welded door.
	constexpr double CubeSize = 100.0;
	constexpr double FloorMinX = 0.0, FloorMaxX = 2000.0, FloorMinY = -2300.0, FloorMaxY = 500.0, SlabThickness = 20.0;
	constexpr double WallX = 1000.0, WallHalfThickness = 20.0, WallHeight = 400.0, DoorHalfWidth = 150.0, WallSouthEnd = -2000.0;
	const FVector RoomA(500.0, 0.0, 150.0);
	const FVector RoomB(1500.0, 0.0, 100.0);
	/** Weld target of the hearing test: a free-standing door facing the welder in room A. */
	const FVector FreeDoorInRoomA(300.0, 0.0, 0.0);
	const FVector WelderInRoomA(500.0, 0.0, 100.0);
	constexpr float TickSeconds = 1.f / 60.f;
	/** Test data: an armored victim proves the hit went through UHollowDamageExecution. */
	constexpr float VictimArmor = 100.f;
	/** Upper bounds of the simulated scenarios. */
	constexpr float BreachScenarioSeconds = 60.f;
	constexpr float StepSeconds = 10.f;
	/** Hearing test: quiet first, then welding until it comes round the wall (the detour is about 55 m). */
	constexpr float QuietSeconds = 4.f;
	constexpr float HearSeconds = 3.f;
	constexpr float ComeSeconds = 45.f;
	constexpr double StimulusTolerance = 50.0;

	inline void Advance(FTestWorldWrapper& World, const float Seconds, const TFunctionRef<bool()> Until = [] { return false; })
	{
		// One engine frame per step: the timer manager ticks once per GFrameCounter.
		for (float Elapsed = 0.f; Elapsed < Seconds && !Until(); Elapsed += TickSeconds)
		{
			++GFrameCounter;
			World.TickTestWorld(TickSeconds);
		}
	}

	inline void SpawnBox(UWorld* World, const FVector& Min, const FVector& Max)
	{
		const FTransform Transform(FQuat::Identity, (Min + Max) * 0.5, (Max - Min) / CubeSize);
		AStaticMeshActor* Box = World->SpawnActorDeferred<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform);
		Box->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
		Box->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, CubeMesh));
		Box->FinishSpawning(Transform);
	}

	enum class EWall : uint8
	{
		/** Open floor; BP_Door stands free in a corner. */
		None,
		/** The wall with BP_Door in its opening. */
		WithDoor,
		/** The wall without an opening (no sight through, only the long detour); BP_Door stands free in room A. */
		Solid
	};

	/**
	 * A navigable test world: floor, optionally the wall, BP_Door, the navmesh built, AI perception started; then
	 * "begun", so everything spawned afterwards (enemies, their controllers, players) plays and ticks like in a raid.
	 */
	struct FEnemyScene
	{
		FTestWorldWrapper TestWorld;
		UWorld* World = nullptr;
		AHollowDoor* Door = nullptr;

		bool Create(FAutomationTestBase& Test, const EWall Wall)
		{
			if (!TestWorld.CreateTestWorld(EWorldType::Game))
			{
				TestWorld.ForwardErrorMessages(&Test);
				return false;
			}
			World = TestWorld.GetTestWorld();
			Test.AddExpectedMessage(TEXT("Unable to find RecastNavMesh instance while trying to create UCrowdManager"), ELogVerbosity::Warning,
				EAutomationExpectedMessageFlags::Contains, /*Occurrences=*/0, /*IsRegex=*/false);
			FNavigationSystem::AddNavigationSystemToWorld(*World, FNavigationSystemRunMode::GameMode);
			World->InitializeActorsForPlay(FURL());
			UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
			const TSubclassOf<AHollowDoor> DoorBlueprint = LoadClass<AHollowDoor>(nullptr, DoorClass);
			if (!Test.TestNotNull(TEXT("Navigation"), Navigation) || !Test.TestNotNull(TEXT("BP_Door"), DoorBlueprint.Get())
				|| !Test.TestNotNull(TEXT("GCN_Enemy_Windup"), LoadClass<AActor>(nullptr, WindupCueClass)))   // loaded now, not into a later test's world
			{
				return false;
			}
			SpawnBox(World, FVector(FloorMinX, FloorMinY, -SlabThickness), FVector(FloorMaxX, FloorMaxY, 0.0));
			if (Wall == EWall::WithDoor)
			{
				SpawnBox(World, FVector(WallX - WallHalfThickness, DoorHalfWidth, 0.0), FVector(WallX + WallHalfThickness, FloorMaxY, WallHeight));
				SpawnBox(World, FVector(WallX - WallHalfThickness, WallSouthEnd, 0.0), FVector(WallX + WallHalfThickness, -DoorHalfWidth, WallHeight));
			}
			else if (Wall == EWall::Solid)
			{
				SpawnBox(World, FVector(WallX - WallHalfThickness, WallSouthEnd, 0.0), FVector(WallX + WallHalfThickness, FloorMaxY, WallHeight));
			}
			const FVector DoorAt = Wall == EWall::WithDoor ? FVector(WallX, 0.0, 0.0)
				: Wall == EWall::Solid ? FreeDoorInRoomA : FVector(FloorMinX + 2.0 * CubeSize, FloorMinY + CubeSize, 0.0);
			Door = World->SpawnActor<AHollowDoor>(DoorBlueprint, FTransform(DoorAt));
			Door->DispatchBeginPlay();
			Navigation->bWholeWorldNavigable = true;   // no NavMeshBoundsVolume in a test world
			Navigation->Build();
			for (ANavigationData* NavData : Navigation->NavDataSet)
			{
				NavData->DispatchBeginPlay();   // they process path requests in their tick
			}
			// Test worlds are made without AI; a raid's world creates it on init and starts it in BeginPlay (perception).
			if (UAISystemBase* AISystem = World->GetAISystem() ? World->GetAISystem() : World->CreateAISystem())
			{
				AISystem->StartPlay();
			}
			World->SetBegunPlay(true);
			return true;
		}

		/** An enemy blueprint, possessed by its AI controller as soon as it spawns, its brain running. */
		AHollowEnemyCharacter* SpawnEnemy(FAutomationTestBase& Test, const TCHAR* ClassPath, const FVector& Location) const
		{
			const TSubclassOf<AHollowEnemyCharacter> Class = LoadClass<AHollowEnemyCharacter>(nullptr, ClassPath);
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AHollowEnemyCharacter* Enemy = Class ? World->SpawnActor<AHollowEnemyCharacter>(Class, Location, FRotator::ZeroRotator, Params) : nullptr;
			Test.TestTrue(FString::Printf(TEXT("%s possessed by its brain"), ClassPath), Enemy && Cast<AHollowAIController>(Enemy->GetController()));
			return Enemy;
		}

		/** BP_IronPilgrim, possessed by its AI controller as soon as it spawns, its brain running. */
		AHollowEnemyCharacter* SpawnPilgrim(FAutomationTestBase& Test, const FVector& Location) const
		{
			return SpawnEnemy(Test, PilgrimClass, Location);
		}

		/** A possessed ExoFrame player with initialized attributes (real GE), optionally armored. */
		UHollowAbilitySystemComponent* SpawnPlayer(FAutomationTestBase& Test, const FVector& Location, AHollowExoFrameCharacter*& OutCharacter,
			const float Armor = 0.f, const bool bLocalPlayer = false) const
		{
			OutCharacter = HollowGameTests::SpawnExoFrame(Test, World, FTransform(Location));
			if (!OutCharacter)
			{
				return nullptr;
			}
			if (bLocalPlayer)
			{
				// A local player sets up input; the bare C++ class has no InputConfig (BP_ExoFrame data), which it reports.
				Test.AddExpectedMessage(TEXT("InputConfig is not set"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, /*Occurrences=*/0, /*IsRegex=*/false);
			}
			HollowGameTests::PossessAsPlayer(World, OutCharacter, bLocalPlayer);
			UHollowAbilitySystemComponent* AbilitySystem = OutCharacter->GetHollowAbilitySystemComponent();
			const TSubclassOf<UGameplayEffect> Init = LoadClass<UGameplayEffect>(nullptr, InitEffectClass);
			if (!AbilitySystem || !Init)
			{
				return nullptr;
			}
			AbilitySystem->ApplyGameplayEffectToSelf(Init.GetDefaultObject(), 1.f, AbilitySystem->MakeEffectContext());
			// Armor through an instant Override effect, like an authored one.
			UGameplayEffect* ArmorEffect = NewObject<UGameplayEffect>(GetTransientPackage());
			ArmorEffect->DurationPolicy = EGameplayEffectDurationType::Instant;
			FGameplayModifierInfo& Modifier = ArmorEffect->Modifiers.AddDefaulted_GetRef();
			Modifier.Attribute = UHollowHealthSet::GetArmorAttribute();
			Modifier.ModifierOp = EGameplayModOp::Override;
			Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Armor));
			AbilitySystem->ApplyGameplayEffectToSelf(ArmorEffect, 1.f, AbilitySystem->MakeEffectContext());
			return AbilitySystem;
		}
	};

	inline float Health(const UAbilitySystemComponent* AbilitySystem)
	{
		return AbilitySystem->GetNumericAttribute(UHollowHealthSet::GetHealthAttribute());
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
