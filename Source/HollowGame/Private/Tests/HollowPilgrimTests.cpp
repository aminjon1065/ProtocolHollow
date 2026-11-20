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

namespace HollowPilgrimTests
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

	void Advance(FTestWorldWrapper& World, const float Seconds, const TFunctionRef<bool()> Until = [] { return false; })
	{
		// One engine frame per step: the timer manager ticks once per GFrameCounter.
		for (float Elapsed = 0.f; Elapsed < Seconds && !Until(); Elapsed += TickSeconds)
		{
			++GFrameCounter;
			World.TickTestWorld(TickSeconds);
		}
	}

	void SpawnBox(UWorld* World, const FVector& Min, const FVector& Max)
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
	struct FPilgrimScene
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

		/** BP_IronPilgrim, possessed by its AI controller as soon as it spawns, its brain running. */
		AHollowEnemyCharacter* SpawnPilgrim(FAutomationTestBase& Test, const FVector& Location) const
		{
			const TSubclassOf<AHollowEnemyCharacter> Class = LoadClass<AHollowEnemyCharacter>(nullptr, PilgrimClass);
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AHollowEnemyCharacter* Pilgrim = Class ? World->SpawnActor<AHollowEnemyCharacter>(Class, Location, FRotator::ZeroRotator, Params) : nullptr;
			Test.TestTrue(TEXT("BP_IronPilgrim possessed by its brain"), Pilgrim && Cast<AHollowAIController>(Pilgrim->GetController()));
			return Pilgrim;
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

	float Health(const UAbilitySystemComponent* AbilitySystem)
	{
		return AbilitySystem->GetNumericAttribute(UHollowHealthSet::GetHealthAttribute());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowPilgrimMeleeTest, "Hollow.Game.Pilgrim.Melee",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowPilgrimMeleeTest::RunTest(const FString& Parameters)
{
	// The strike is telegraphed (wind-up cue), lands at the end of the wind-up and only on living players in front,
	// within reach and sight; the damage goes through the attack's effect and UHollowDamageExecution (armor counts).
	using namespace HollowPilgrimTests;
	FPilgrimScene Scene;
	const UHollowEnemyAttackDefinition* Attack = LoadObject<UHollowEnemyAttackDefinition>(nullptr, MeleeAttack);
	if (!TestNotNull(TEXT("DA_Attack_Pilgrim_Melee"), Attack) || !Scene.Create(*this, EWall::None))
	{
		return false;
	}
	AHollowEnemyCharacter* Pilgrim = Scene.SpawnPilgrim(*this, RoomA);
	if (!Pilgrim)
	{
		return false;
	}
	Cast<AHollowAIController>(Pilgrim->GetController())->Shutdown();   // the attack alone, no brain deciding meanwhile
	Advance(Scene.TestWorld, StepSeconds * 0.1f);   // land on the floor

	const double Radius = Pilgrim->GetSimpleCollisionRadius();
	const FVector Forward = Pilgrim->GetActorForwardVector();
	const FVector Center = Pilgrim->GetActorLocation();
	AHollowExoFrameCharacter* Victim = nullptr;
	AHollowExoFrameCharacter* Behind = nullptr;
	AHollowExoFrameCharacter* Far = nullptr;
	UHollowAbilitySystemComponent* VictimAbilities = Scene.SpawnPlayer(*this, Center + Forward * (Radius + Attack->Reach * 0.5), Victim, VictimArmor);
	UHollowAbilitySystemComponent* BehindAbilities = Scene.SpawnPlayer(*this, Center - Forward * (Radius + Attack->Reach * 0.5), Behind);
	UHollowAbilitySystemComponent* FarAbilities = Scene.SpawnPlayer(*this, Center + Forward * (Radius + Attack->Reach * 3.0) + FVector(0.0, Attack->Reach, 0.0), Far);
	if (!VictimAbilities || !BehindAbilities || !FarAbilities)
	{
		return false;
	}
	const float FullHealth = Health(VictimAbilities);

	FGameplayEventData Payload;
	Payload.Instigator = Pilgrim;
	Payload.Target = Victim;
	UAbilitySystemComponent* PilgrimAbilities = Pilgrim->GetAbilitySystemComponent();
	TestEqual(TEXT("The melee event triggers the strike"), PilgrimAbilities->HandleGameplayEvent(HollowTags::Event_Enemy_Melee, &Payload), 1);
	Advance(Scene.TestWorld, Attack->WindupSeconds * 0.5f);
	TestTrue(TEXT("Winding up: the telegraph is on"), PilgrimAbilities->HasMatchingGameplayTag(HollowTags::GameplayCue_Enemy_Windup));
	TestEqual(TEXT("...and nobody is hit yet"), Health(VictimAbilities), FullHealth);
	Advance(Scene.TestWorld, Attack->WindupSeconds);

	const UHollowDamageSettings* Settings = GetDefault<UHollowDamageSettings>();
	const FHollowDamageTypeRule* Kinetic = Settings->FindRule(HollowTags::Damage_Kinetic);
	const float Expected = HollowDamageMath::Mitigate(Attack->Damage, VictimArmor, Settings->ArmorConstant, Kinetic ? Kinetic->bArmorApplies : true, false);
	TestFalse(TEXT("The telegraph ends with the blow"), PilgrimAbilities->HasMatchingGameplayTag(HollowTags::GameplayCue_Enemy_Windup));
	TestTrue(FString::Printf(TEXT("The victim takes the executed damage (%.1f, armor counted)"), Expected),
		Expected < Attack->Damage && FMath::IsNearlyEqual(Health(VictimAbilities), FullHealth - Expected, KINDA_SMALL_NUMBER));
	TestEqual(TEXT("Nobody behind it is hit"), Health(BehindAbilities), FullHealth);
	TestEqual(TEXT("Nobody out of reach is hit"), Health(FarAbilities), FullHealth);

	Advance(Scene.TestWorld, Attack->RecoverySeconds + TickSeconds * 4.f);
	TestTrue(TEXT("Recovered: it can strike again"), PilgrimAbilities->HandleGameplayEvent(HollowTags::Event_Enemy_Melee, &Payload) == 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowPilgrimBreachDoorTest, "Hollow.Game.Pilgrim.BreachDoor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowPilgrimBreachDoorTest::RunTest(const FString& Parameters)
{
	// The acceptance scenario of S3-04: the player is behind a fully welded door; the Pilgrim's Heavy path goes through
	// the door's nav link, path following stops it at the link, the brain switches to Breach, its structural blows break
	// the door, the link turns into a doorway, and it walks through to strike the player.
	using namespace HollowPilgrimTests;
	FPilgrimScene Scene;
	if (!Scene.Create(*this, EWall::WithDoor))
	{
		return false;
	}
	UHollowDoorStateComponent* DoorState = Scene.Door->GetDoorState();
	const UHollowDoorDefinition* DoorDefinition = DoorState->GetDefinition();
	DoorState->ApplyTool(EHollowDoorEvent::Weld, 1.f / DoorDefinition->WeldPerWorkUnit);
	TestTrue(TEXT("The door is fully welded"), DoorState->GetNetState().State == EHollowDoorState::Welded && DoorState->GetWeld() == 1.f);

	AHollowExoFrameCharacter* Player = nullptr;
	UHollowAbilitySystemComponent* PlayerAbilities = Scene.SpawnPlayer(*this, RoomB, Player);
	AHollowEnemyCharacter* Pilgrim = Scene.SpawnPilgrim(*this, RoomA);
	AHollowAIController* Brain = Pilgrim ? Cast<AHollowAIController>(Pilgrim->GetController()) : nullptr;
	if (!PlayerAbilities || !Brain)
	{
		return false;
	}
	const float FullHealth = Health(PlayerAbilities);
	Brain->ReportSeen(Player);   // the senses are the perception system's business; here the brain is told

	bool bStoppedAtDoor = false;
	bool bBreachedBeforeCrossing = true;
	Advance(Scene.TestWorld, BreachScenarioSeconds, [&]
	{
		bStoppedAtDoor |= Brain->GetActivity() == EHollowAIActivity::Breach && Brain->GetObstacle() == Scene.Door;
		bBreachedBeforeCrossing &= Pilgrim->GetActorLocation().X < WallX || DoorState->GetNetState().State == EHollowDoorState::Breached;
		return Health(PlayerAbilities) < FullHealth;
	});
	TestTrue(TEXT("Stopped at the welded door's link and set about breaking it"), bStoppedAtDoor);
	TestTrue(TEXT("Breached by its blows"), DoorState->GetNetState().State == EHollowDoorState::Breached);
	TestTrue(TEXT("The link is a doorway now"), Scene.Door->GetNavArea() == DoorDefinition->AreaDoorway);
	TestTrue(TEXT("Never passed the door before it broke"), bBreachedBeforeCrossing);
	TestTrue(TEXT("Through the door"), Pilgrim->GetActorLocation().X > WallX);
	TestTrue(TEXT("...and the player behind it was struck"), Health(PlayerAbilities) < FullHealth);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowPilgrimBrainTest, "Hollow.Game.Pilgrim.Brain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowPilgrimBrainTest::RunTest(const FString& Parameters)
{
	// ST_IronPilgrim picks the activity from the controller's facts, in priority order: an obstacle in the way beats
	// a target in range (attack), which beats a target (chase), a stimulus (investigate) and patrol. Death stops it.
	using namespace HollowPilgrimTests;
	FPilgrimScene Scene;
	if (!Scene.Create(*this, EWall::None))
	{
		return false;
	}
	AHollowEnemyCharacter* Pilgrim = Scene.SpawnPilgrim(*this, RoomA);
	AHollowAIController* Brain = Pilgrim ? Cast<AHollowAIController>(Pilgrim->GetController()) : nullptr;
	const UHollowEnemyDefinition* Definition = Pilgrim ? Pilgrim->GetDefinition() : nullptr;
	if (!Brain || !TestNotNull(TEXT("Definition"), Definition) || !TestTrue(TEXT("ST_IronPilgrim is compiled"), Definition->StateTree && Definition->StateTree->IsReadyToRun()))
	{
		return false;
	}
	auto Is = [Brain](const EHollowAIActivity Activity) { return Brain->GetActivity() == Activity; };
	auto InState = [Brain](const TCHAR* State) { return Brain->GetBrain()->GetActiveStateNames().Contains(FName(State)); };

	Advance(Scene.TestWorld, TickSeconds * 4.f);
	TestTrue(TEXT("Nothing known: patrol"), Is(EHollowAIActivity::Patrol) && InState(TEXT("Patrol")));

	Brain->ReportHeard(RoomA + FVector(CubeSize * 6.0, 0.0, 0.0));
	Advance(Scene.TestWorld, TickSeconds * 4.f);
	TestTrue(TEXT("A noise: investigate"), Is(EHollowAIActivity::Investigate) && InState(TEXT("Investigate")));

	AHollowExoFrameCharacter* Player = nullptr;
	UHollowAbilitySystemComponent* PlayerAbilities = Scene.SpawnPlayer(*this, RoomA + FVector(CubeSize * 12.0, 0.0, 0.0), Player);
	if (!PlayerAbilities)
	{
		return false;
	}
	Brain->ReportSeen(Player);
	Advance(Scene.TestWorld, TickSeconds * 4.f);
	TestTrue(TEXT("A player in sight: chase"), Is(EHollowAIActivity::Chase) && InState(TEXT("Chase")) && Brain->GetTarget() == Player);

	Advance(Scene.TestWorld, StepSeconds, [&] { return Is(EHollowAIActivity::Attack); });
	TestTrue(TEXT("Caught up: attack"), Is(EHollowAIActivity::Attack) && InState(TEXT("Attack")) && Brain->IsTargetInEngageRange());

	Scene.Door->GetDoorState()->ApplyTool(EHollowDoorEvent::Weld, 1.f / Scene.Door->GetDoorState()->GetDefinition()->WeldPerWorkUnit);
	Scene.Door->SetActorLocation(Pilgrim->GetActorLocation() - Pilgrim->GetActorForwardVector() * (Pilgrim->GetSimpleCollisionRadius() + CubeSize));
	Brain->ReportObstacle(Scene.Door);
	Advance(Scene.TestWorld, StepSeconds, [&] { return Is(EHollowAIActivity::Breach); });
	TestTrue(TEXT("An obstacle in the way beats the target"), Is(EHollowAIActivity::Breach) && InState(TEXT("Breach")));
	Advance(Scene.TestWorld, StepSeconds, [&] { return Scene.Door->GetDoorState()->GetWeld() < 1.f; });
	TestTrue(TEXT("...and it strikes the obstacle"), Scene.Door->GetDoorState()->GetWeld() < 1.f);

	Pilgrim->Die();
	Advance(Scene.TestWorld, TickSeconds * 4.f);
	TestTrue(TEXT("Dead: the brain stops"), Pilgrim->IsDead() && !Brain->GetBrain()->IsRunning() && Is(EHollowAIActivity::None));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowPilgrimHearsWeldingTest, "Hollow.Game.Pilgrim.HearsWelding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowPilgrimHearsWeldingTest::RunTest(const FString& Parameters)
{
	// The S3-05 acceptance: the Pilgrim comes to the sound of real welding. It stands behind a solid wall (no sight)
	// while a player welds a door with the real welder ability; the noise goes through UHollowNoiseSubsystem and real
	// AI hearing; the Pilgrim investigates there, walks round the wall and finds the welder. Silence brings nothing.
	using namespace HollowPilgrimTests;
	FPilgrimScene Scene;
	const TSubclassOf<UGameplayAbility> WelderClass = LoadClass<UGameplayAbility>(nullptr, WelderAbilityClass);
	if (!TestNotNull(TEXT("GA_Tool_Welder"), WelderClass.Get()) || !TestNotNull(TEXT("GCN_Tool_Beam"), LoadClass<AActor>(nullptr, BeamCueClass))
		|| !Scene.Create(*this, EWall::Solid) || !TestNotNull(TEXT("AI system with perception"), Scene.World->GetAISystem()))
	{
		return false;
	}
	AHollowExoFrameCharacter* Welder = nullptr;
	UHollowAbilitySystemComponent* WelderAbilities = Scene.SpawnPlayer(*this, WelderInRoomA, Welder, 0.f, /*bLocalPlayer=*/true);
	AHollowEnemyCharacter* Pilgrim = Scene.SpawnPilgrim(*this, RoomB);
	AHollowAIController* Brain = Pilgrim ? Cast<AHollowAIController>(Pilgrim->GetController()) : nullptr;
	if (!WelderAbilities || !Brain)
	{
		return false;
	}
	Welder->GetController()->SetControlRotation((FreeDoorInRoomA - WelderInRoomA).Rotation());   // facing the door it welds
	FGameplayAbilitySpec Spec(WelderClass, 1);
	Spec.GetDynamicSpecSourceTags().AddTag(HollowTags::Input_SecondaryUse);
	WelderAbilities->GiveAbility(Spec);

	bool bInvestigated = false;
	Advance(Scene.TestWorld, QuietSeconds, [&] { bInvestigated |= Brain->GetActivity() == EHollowAIActivity::Investigate; return false; });
	TestFalse(TEXT("Quiet: nothing to investigate"), bInvestigated || Brain->HasStimulus());

	WelderAbilities->AbilityInputTagPressed(HollowTags::Input_SecondaryUse);
	Advance(Scene.TestWorld, HearSeconds, [&] { return Brain->GetActivity() == EHollowAIActivity::Investigate; });
	TestTrue(TEXT("Welding: it heard and investigates"), Brain->GetActivity() == EHollowAIActivity::Investigate && !Brain->GetTarget());
	TestTrue(TEXT("...where the welder is"), FVector::Dist2D(Brain->GetStimulusLocation(), Welder->GetActorLocation()) < StimulusTolerance);

	double SouthMost = Pilgrim->GetActorLocation().Y;
	Advance(Scene.TestWorld, ComeSeconds, [&] { SouthMost = FMath::Min(SouthMost, Pilgrim->GetActorLocation().Y); return Brain->GetTarget() == Welder; });
	WelderAbilities->AbilityInputTagReleased(HollowTags::Input_SecondaryUse);
	TestTrue(TEXT("It came round the wall"), SouthMost < WallSouthEnd);
	TestTrue(TEXT("...and found the welder"), Brain->GetTarget() == Welder);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
