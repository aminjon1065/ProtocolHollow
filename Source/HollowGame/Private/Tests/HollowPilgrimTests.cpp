#include "Tests/HollowEnemyTestScene.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace HollowPilgrimTests
{
	using namespace HollowEnemyTests;
	using FPilgrimScene = FEnemyScene;
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
