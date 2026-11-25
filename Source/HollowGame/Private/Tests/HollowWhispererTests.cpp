#include "Tests/HollowEnemyTestScene.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Audio/HollowAudioSubsystem.h"
#include "Audio/HollowSoundBank.h"
#include "Brain/HollowWhispererController.h"
#include "Components/AudioComponent.h"
#include "Components/SpotLightComponent.h"
#include "Data/HollowBiomeDefinition.h"
#include "Data/HollowLanternDefinition.h"
#include "Enemies/HollowWhisperer.h"
#include "Enemies/HollowWhispererDefinition.h"
#include "Enemies/HollowWhispererNest.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Interfaces/HollowStructural.h"
#include "Items/HollowItemDefinition.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "Sound/SoundBase.h"
#include "Threat/HollowSwarmRules.h"
#include "Threat/HollowSwarmSubsystem.h"
#include "Threat/HollowThreatConfig.h"
#include "Tools/HollowGA_AetherLantern.h"
#include "Weapons/HollowHitMarker.h"
#include "World/HollowEnemyDropSubsystem.h"
#include "World/HollowLootPickup.h"

namespace HollowWhispererTests
{
	using namespace HollowEnemyTests;

	// Content under test (create_whisperer_assets.py, create_art_kit.py, create_audio_assets.py, create_lantern_assets.py).
	const TCHAR* const WhispererClass = TEXT("/Game/Hollow/Characters/Enemies/Whisperer/BP_Whisperer.BP_Whisperer_C");
	const TCHAR* const NestClass = TEXT("/Game/Hollow/Characters/Enemies/Whisperer/BP_WhispererNest.BP_WhispererNest_C");
	const TCHAR* const DefinitionPath = TEXT("/Game/Hollow/Characters/Enemies/Whisperer/DA_Enemy_Whisperer.DA_Enemy_Whisperer");
	const TCHAR* const BiteAttack = TEXT("/Game/Hollow/Characters/Enemies/Whisperer/DA_Attack_Whisperer_Bite.DA_Attack_Whisperer_Bite");
	const TCHAR* const BiteCueClass = TEXT("/Game/Hollow/GAS/Cues/GCN_Whisperer_Windup.GCN_Whisperer_Windup_C");
	const TCHAR* const KineticEffect = TEXT("/Game/Hollow/GAS/Effects/GE_Damage_Kinetic.GE_Damage_Kinetic_C");
	const TCHAR* const LanternAbilityClass = TEXT("/Game/Hollow/Tools/AetherLantern/GA_Tool_AetherLantern.GA_Tool_AetherLantern_C");
	const TCHAR* const LanternCueClass = TEXT("/Game/Hollow/GAS/Cues/GCN_Tool_Lantern.GCN_Tool_Lantern_C");
	const TCHAR* const ExposedCueClass = TEXT("/Game/Hollow/GAS/Cues/GCN_Status_Exposed.GCN_Status_Exposed_C");
	const TCHAR* const PilgrimBlueprint = TEXT("/Game/Hollow/Characters/Enemies/IronPilgrim/BP_IronPilgrim.BP_IronPilgrim_C");
	const TCHAR* const Biomes[] = {TEXT("/HollowSector_Docks/Data/DA_Biome_Docks.DA_Biome_Docks"),
		TEXT("/HollowSector_Cathedral/Data/DA_Biome_Cathedral.DA_Biome_Cathedral")};
	const FPrimaryAssetId AetherShard(UHollowItemDefinition::AssetType, TEXT("DA_Item_AetherShard"));

	// The scene (FEnemyScene's open floor, uu): the player looks along +X; a Whisperer 6 m ahead in its lamp, one to its
	// side out of it.
	const FVector PlayerAt(300.0, -1000.0, 100.0);
	const FVector AheadAt(900.0, -1000.0, 40.0);
	const FVector AsideAt(300.0, -1700.0, 40.0);
	const FVector NestAt(1200.0, -1000.0, 0.0);
	/** The shipped lamp of BP_ExoFrame (test data, as in the stalker tests): the C++ class's own is narrower. */
	constexpr float LampHalfAngle = 26.f;
	constexpr float LampRange = 1800.f;
	constexpr float Settle = 0.05f;
	constexpr float SpeedTolerance = 0.01f;
	/** How many seeds the statistical rules look at. */
	constexpr int32 Seeds = 1000;

	const UHollowWhispererDefinition* Definition()
	{
		return LoadObject<UHollowWhispererDefinition>(nullptr, DefinitionPath);
	}

	float Seconds(const TSubclassOf<UGameplayEffect>& Effect)
	{
		float Value = 0.f;
		return Effect && Effect.GetDefaultObject()->DurationMagnitude.GetStaticMagnitudeIfPossible(1.f, Value) ? Value : 0.f;
	}

	/** The speed multiplier an effect's one MultiplyCompound modifier sets (its data). */
	float Multiplier(const TSubclassOf<UGameplayEffect>& Effect)
	{
		float Value = 0.f;
		const UGameplayEffect* Defaults = Effect ? Effect.GetDefaultObject() : nullptr;
		return Defaults && Defaults->Modifiers.Num() == 1 && Defaults->Modifiers[0].ModifierMagnitude.GetStaticMagnitudeIfPossible(1.f, Value) ? Value : 0.f;
	}

	void Aim(AHollowExoFrameCharacter* Player, const FVector& At)
	{
		Player->GetController()->SetControlRotation((At - Player->GetPawnViewLocation()).Rotation());
		Player->GetHelmetLight()->SetOuterConeAngle(LampHalfAngle);
		Player->GetHelmetLight()->SetAttenuationRadius(LampRange);
	}

	AHollowWhispererController* BrainOf(const AHollowEnemyCharacter* Whisperer)
	{
		return Whisperer ? Cast<AHollowWhispererController>(Whisperer->GetController()) : nullptr;
	}

	bool Has(const AHollowEnemyCharacter* Enemy, const FGameplayTag& Tag)
	{
		return Enemy && Enemy->GetAbilitySystemComponent()->HasMatchingGameplayTag(Tag);
	}

	void ApplyFrom(UAbilitySystemComponent* Source, AHollowEnemyCharacter* Target, const TSubclassOf<UGameplayEffect>& Effect, const float Damage)
	{
		const FGameplayEffectSpecHandle Spec = Source->MakeOutgoingSpec(Effect, 1.f, Source->MakeEffectContext());
		Spec.Data->SetSetByCallerMagnitude(HollowTags::Data_Damage, Damage);
		Source->ApplyGameplayEffectSpecToTarget(*Spec.Data, Target->GetAbilitySystemComponent());
	}

	/** A nest with Seed at Location, its brood out (the tick after it begins play). */
	AHollowWhispererNest* SpawnNest(FAutomationTestBase& Test, FEnemyScene& Scene, const FVector& Location, const int32 Seed)
	{
		const TSubclassOf<AHollowWhispererNest> Class = LoadClass<AHollowWhispererNest>(nullptr, NestClass);
		const FTransform Transform(Location);
		AHollowWhispererNest* Nest = Class ? Scene.World->SpawnActorDeferred<AHollowWhispererNest>(Class, Transform) : nullptr;
		if (!Test.TestNotNull(TEXT("BP_WhispererNest"), Nest))
		{
			return nullptr;
		}
		Nest->SetSpawnSeed(Seed);
		Nest->FinishSpawning(Transform);
		Advance(Scene.TestWorld, Settle);
		return Nest;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowSwarmRulesTest, "Hollow.AI.Swarm.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowSwarmRulesTest::RunTest(const FString& Parameters)
{
	// GP-02 (ADR-0047): a nest's brood is 2..4, the same for the same seed and spread over seeds; its members get their
	// own seeds and come out evenly round it; two bite a lit player, a frenzy bites freely; the frenzy is the dark with
	// prey whose light is off; a flinch backs straight away from the light; the prey in the dark is the dearer one; a
	// drop rolls its chance, the same for the same seed.
	using namespace HollowWhispererTests;
	TArray<int32> SizeCounts;
	SizeCounts.Init(0, 5);
	bool bInRange = true;
	bool bStable = true;
	for (int32 Seed = 0; Seed < Seeds; ++Seed)
	{
		const int32 Size = HollowSwarmRules::BroodSize(Seed, 2, 4);
		bInRange &= Size >= 2 && Size <= 4;
		bStable &= HollowSwarmRules::BroodSize(Seed, 2, 4) == Size;
		if (Size >= 0 && Size < SizeCounts.Num())
		{
			++SizeCounts[Size];
		}
	}
	TestTrue(TEXT("A brood of 2..4"), bInRange);
	TestTrue(TEXT("...the same for the same seed"), bStable);
	TestTrue(FString::Printf(TEXT("...every size turns up (%d / %d / %d)"), SizeCounts[2], SizeCounts[3], SizeCounts[4]),
		SizeCounts[2] > Seeds / 5 && SizeCounts[3] > Seeds / 5 && SizeCounts[4] > Seeds / 5);
	TestEqual(TEXT("Min above max: the min"), HollowSwarmRules::BroodSize(7, 3, 2), 3);

	constexpr int32 NestSeed = 1234;
	TSet<int32> MemberSeeds;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		MemberSeeds.Add(HollowSwarmRules::MemberSeed(NestSeed, Index));
	}
	TestEqual(TEXT("Each member its own seed"), MemberSeeds.Num(), 4);
	TestEqual(TEXT("...the same for the same nest"), HollowSwarmRules::MemberSeed(NestSeed, 2), HollowSwarmRules::MemberSeed(NestSeed, 2));

	const FVector Center(100.0, 200.0, 0.0);
	constexpr double Radius = 150.0;
	const FVector First = HollowSwarmRules::BroodSpot(NestSeed, 0, 3, Center, Radius);
	const FVector Second = HollowSwarmRules::BroodSpot(NestSeed, 1, 3, Center, Radius);
	TestTrue(TEXT("Brood spots on the ring"), FMath::IsNearlyEqual(FVector::Dist(First, Center), Radius, 0.01) && FMath::IsNearlyEqual(FVector::Dist(Second, Center), Radius, 0.01));
	TestTrue(TEXT("...evenly apart (120 degrees for three)"), FMath::IsNearlyEqual(FVector::Dist(First, Second), 2.0 * Radius * FMath::Sin(UE_DOUBLE_PI / 3.0), 0.01));
	TestTrue(TEXT("...the same for the same seed"), HollowSwarmRules::BroodSpot(NestSeed, 1, 3, Center, Radius).Equals(Second));

	TestTrue(TEXT("Under the limit: it bites"), HollowSwarmRules::CanBite(1, 2, false));
	TestFalse(TEXT("At the limit: it waits"), HollowSwarmRules::CanBite(2, 2, false));
	TestTrue(TEXT("In a frenzy: it bites anyway"), HollowSwarmRules::CanBite(5, 2, true));

	TestTrue(TEXT("Dark, prey without light: the frenzy"), HollowSwarmRules::ShouldFrenzy(false, false, true, false));
	TestFalse(TEXT("...not in a beam"), HollowSwarmRules::ShouldFrenzy(true, false, true, false));
	TestFalse(TEXT("...nor exposed"), HollowSwarmRules::ShouldFrenzy(false, true, true, false));
	TestFalse(TEXT("...nor without prey"), HollowSwarmRules::ShouldFrenzy(false, false, false, false));
	TestFalse(TEXT("...nor while the prey has a light"), HollowSwarmRules::ShouldFrenzy(false, false, true, true));

	const FVector Goal = HollowSwarmRules::RecoilGoal(FVector(500.0, 0.0, 30.0), FVector(0.0, 0.0, 160.0), 300.0);
	TestTrue(TEXT("A flinch backs straight away from the light, in the plane"), Goal.Equals(FVector(800.0, 0.0, 30.0), 0.01));
	TestTrue(TEXT("...any way out from right under it"), FVector::Dist2D(HollowSwarmRules::RecoilGoal(FVector::ZeroVector, FVector::ZeroVector, 300.0), FVector::ZeroVector) > 299.0);

	TestTrue(TEXT("The prey in the dark: dearer up to twice as far"), HollowSwarmRules::PreyCost(FMath::Square(380.0), false, 2.0) < HollowSwarmRules::PreyCost(FMath::Square(200.0), true, 2.0));
	TestTrue(TEXT("...not farther"), HollowSwarmRules::PreyCost(FMath::Square(500.0), false, 2.0) > HollowSwarmRules::PreyCost(FMath::Square(200.0), true, 2.0));

	FHollowEnemyDrop Drop;
	Drop.Item = AetherShard;
	Drop.Count = 1;
	Drop.Chance = 0.3f;
	Drop.PickupClass = TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/Hollow/World/Loot/BP_LootPickup.BP_LootPickup_C")));
	int32 Rolled = 0;
	bool bSame = true;
	for (int32 Seed = 0; Seed < Seeds; ++Seed)
	{
		Rolled += Drop.Rolls(HollowSwarmRules::MemberSeed(Seed, 0)) ? 1 : 0;
		bSame &= Drop.Rolls(Seed) == Drop.Rolls(Seed);
	}
	TestTrue(FString::Printf(TEXT("A drop rolls its chance (%d of %d)"), Rolled, Seeds), Rolled > Seeds / 4 && Rolled < Seeds * 7 / 20);
	TestTrue(TEXT("...the same for the same seed"), bSame);
	// The members of one brood roll apart: two of them alike about as often as chance says (0.3^2 + 0.7^2 = 0.58).
	int32 Alike = 0;
	for (int32 Seed = 0; Seed < Seeds; ++Seed)
	{
		Alike += Drop.Rolls(HollowSwarmRules::MemberSeed(Seed, 0)) == Drop.Rolls(HollowSwarmRules::MemberSeed(Seed, 1)) ? 1 : 0;
	}
	TestTrue(FString::Printf(TEXT("...each member of a brood on its own (%d of %d pairs alike)"), Alike, Seeds), Alike > Seeds / 2 && Alike < Seeds * 2 / 3);
	Drop.Count = 0;
	TestFalse(TEXT("Nothing to drop: never"), Drop.Rolls(1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowWhispererDataTest, "Hollow.Game.Whisperer.Data",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowWhispererDataTest::RunTest(const FString& Parameters)
{
	// GP-02 content: the Whisperer's brain, light, frenzy and flank data, its bite and drop; the nest's brood; the
	// biome rules that grow nests after the Pilgrim's socket; the swarm tuning; its sounds.
	using namespace HollowWhispererTests;
	const UHollowWhispererDefinition* Whisperer = Definition();
	const UHollowEnemyAttackDefinition* Bite = LoadObject<UHollowEnemyAttackDefinition>(nullptr, BiteAttack);
	const TSubclassOf<AHollowWhisperer> Pawn = LoadClass<AHollowWhisperer>(nullptr, WhispererClass);
	const TSubclassOf<AHollowWhispererNest> Nest = LoadClass<AHollowWhispererNest>(nullptr, NestClass);
	const UHollowThreatConfig* Threat = UHollowThreatConfig::Get();
	const UHollowSoundBank* Bank = UHollowSoundBank::Get();
	if (!TestNotNull(TEXT("DA_Enemy_Whisperer"), Whisperer) || !TestNotNull(TEXT("DA_Attack_Whisperer_Bite"), Bite) || !TestNotNull(TEXT("BP_Whisperer"), Pawn.Get())
		|| !TestNotNull(TEXT("BP_WhispererNest"), Nest.Get()) || !TestNotNull(TEXT("DA_ThreatConfig"), Threat) || !TestNotNull(TEXT("DA_SoundBank"), Bank))
	{
		return false;
	}
	TestTrue(TEXT("A brain, an ability set, the Standard filter"), Whisperer->StateTree && Whisperer->AbilitySet && Whisperer->NavigationFilter);
	TestTrue(TEXT("It opens doors, breaks nothing (a weld keeps it out)"), !Whisperer->CanBreach());
	TestTrue(TEXT("A flank query and its light effects"), Whisperer->FlankQuery && Whisperer->LitEffect && Whisperer->FrenzyEffect);
	const float Lit = Multiplier(Whisperer->LitEffect);
	const float Frenzy = Multiplier(Whisperer->FrenzyEffect);
	TestTrue(FString::Printf(TEXT("A beam slows it (x%.2f), a frenzy speeds it (x%.2f)"), Lit, Frenzy), Lit > 0.f && Lit < 1.f && Frenzy > 1.f);
	TestTrue(TEXT("Both linger past one light check"), Seconds(Whisperer->LitEffect) > Threat->Swarm.LightCheckSeconds
		&& Seconds(Whisperer->FrenzyEffect) > Threat->Swarm.LightCheckSeconds);
	TestTrue(TEXT("It flinches, backs off, flees a lantern farther, not again at once"), Whisperer->FlinchSeconds > 0.f && Whisperer->RecoilDistance > 0.f
		&& Whisperer->ExposedFleeDistance > Whisperer->RecoilDistance && Whisperer->RecoilCooldownSeconds > Whisperer->FlinchSeconds);
	TestTrue(TEXT("It prefers prey in the dark"), Whisperer->LitPreyDistanceScale > 1.f);
	TestTrue(TEXT("A frenzy outruns its chase"), Whisperer->ChaseSpeed * Frenzy > Whisperer->ChaseSpeed && Whisperer->PatrolSpeed < Whisperer->ChaseSpeed);
	TestTrue(TEXT("Its bite reaches its engage range, a short wind-up"), Bite->Reach >= Whisperer->EngageRange && Bite->WindupSeconds < 0.5f && Bite->Damage > 0.f);
	TestTrue(TEXT("Its bite's small glow"), Bite->WindupCue == HollowTags::GameplayCue_Enemy_Windup_Whisperer && LoadClass<AActor>(nullptr, BiteCueClass));
	TestTrue(TEXT("It drops an Aether shard now and then"), Whisperer->Drop.IsSet() && Whisperer->Drop.Item == AetherShard
		&& Whisperer->Drop.Chance > 0.f && Whisperer->Drop.Chance < 1.f);
	const AHollowWhisperer* Body = Pawn.GetDefaultObject();
	TestTrue(TEXT("BP_Whisperer: its definition, its brain, its body mesh"), Body->GetDefinition() == Whisperer
		&& Body->AIControllerClass == AHollowWhispererController::StaticClass() && Body->GetBody()->GetStaticMesh());
	TestTrue(TEXT("...a swarm that keeps apart"), Body->GetCharacterMovement()->bUseRVOAvoidance);
	const AHollowWhispererNest* NestDefaults = Nest.GetDefaultObject();
	TestTrue(TEXT("BP_WhispererNest: a brood of 2..4"), NestDefaults->GetBroodSize() >= 2 && NestDefaults->GetBroodSize() <= 4);
	TestTrue(TEXT("Two bite a lit player at once; the lights checked often"), Threat->Swarm.MaxBitersPerPrey == 2 && Threat->Swarm.LightCheckSeconds <= 0.25f);

	const UClass* Pilgrim = LoadClass<AActor>(nullptr, PilgrimBlueprint);
	for (const TCHAR* Path : Biomes)
	{
		const UHollowBiomeDefinition* Biome = LoadObject<UHollowBiomeDefinition>(nullptr, Path);
		if (!TestNotNull(Path, Biome))
		{
			continue;
		}
		TArray<const FHollowSocketActorRule*> Enemy;
		for (const FHollowSocketActorRule& Rule : Biome->SocketActors)
		{
			if (Rule.SocketType == HollowTags::Socket_EnemySpawn)
			{
				Enemy.Add(&Rule);
			}
		}
		const bool bOrder = Enemy.Num() >= 3 && Enemy[0]->ActorClass.Get() == Pilgrim && Enemy[1]->ActorClass.Get() == Nest.Get();
		TestTrue(FString::Printf(TEXT("%s: the Pilgrim's socket first, then the nests"), Path), bOrder);
		TestTrue(FString::Printf(TEXT("%s: up to two nests, two rooms or more from Spawn"), Path), bOrder && Enemy[1]->MaxPerSector == 2
			&& Enemy[1]->MinRoomDistance == 2);
	}

	const TArray<FGameplayTag> Sounds = {HollowTags::Sound_Whisperer_Whisper, HollowTags::Sound_Whisperer_Shriek, HollowTags::Sound_Whisperer_Bite,
		HollowTags::Sound_Whisperer_Death, HollowTags::Sound_Footstep_Whisperer};
	for (const FGameplayTag& Tag : Sounds)
	{
		TestNotNull(*FString::Printf(TEXT("%s in the bank"), *Tag.ToString()), Bank->Find(Tag));
	}
	TestTrue(TEXT("Its bite and its steps are its own"), Whisperer->WindupSound == HollowTags::Sound_Whisperer_Bite
		&& Whisperer->FootstepSound == HollowTags::Sound_Footstep_Whisperer);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowWhispererNestTest, "Hollow.Game.Whisperer.Nest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowWhispererNestTest::RunTest(const FString& Parameters)
{
	// A nest's brood comes out the tick after it begins play: as many as its seed says, on the navmesh round it, each
	// a Whisperer with its own brain and seed, one swarm group; another nest with the same seed holds as many; when one
	// sees a player the rest hear where it is.
	using namespace HollowWhispererTests;
	FEnemyScene Scene;
	if (!Scene.Create(*this, EWall::None))
	{
		return false;
	}
	constexpr int32 Seed = 77;
	AHollowWhispererNest* Nest = SpawnNest(*this, Scene, NestAt, Seed);
	UHollowSwarmSubsystem* Swarm = UHollowSwarmSubsystem::Get(Scene.World);
	if (!Nest || !TestNotNull(TEXT("Swarm subsystem"), Swarm))
	{
		return false;
	}
	TestTrue(TEXT("Its brood is out"), Nest->HasSpawned());
	TestEqual(TEXT("...as many as its seed holds"), Nest->GetBrood().Num(), Nest->GetBroodSize());
	TestTrue(TEXT("...2 to 4"), Nest->GetBroodSize() >= 2 && Nest->GetBroodSize() <= 4);
	TSet<int32> MemberSeeds;
	TSet<int32> Groups;
	bool bWhisperers = true;
	bool bNear = true;
	for (const TWeakObjectPtr<AHollowEnemyCharacter>& Weak : Nest->GetBrood())
	{
		const AHollowEnemyCharacter* Member = Weak.Get();
		bWhisperers &= Member && Member->IsA<AHollowWhisperer>() && BrainOf(Member) != nullptr;
		bNear &= Member && FVector::Dist2D(Member->GetActorLocation(), NestAt) < 400.0;
		if (Member)
		{
			MemberSeeds.Add(Member->GetSpawnSeed());
			Groups.Add(Swarm->GetGroupOf(Member));
		}
	}
	TestTrue(TEXT("...Whisperers with their brains"), bWhisperers);
	TestTrue(TEXT("...round the nest"), bNear);
	TestEqual(TEXT("...each its own seed"), MemberSeeds.Num(), Nest->GetBroodSize());
	TestTrue(TEXT("...one swarm group"), Groups.Num() == 1 && !Groups.Contains(INDEX_NONE));
	TestEqual(TEXT("The swarm knows them all"), Swarm->GetWhispererCount(), Nest->GetBroodSize());
	TestEqual(TEXT("It spawns its brood once"), Nest->SpawnBrood(), 0);

	AHollowWhispererNest* Twin = SpawnNest(*this, Scene, NestAt + FVector(0.0, 800.0, 0.0), Seed);
	TestTrue(TEXT("The same seed: the same brood"), Twin && Twin->GetBrood().Num() == Nest->GetBrood().Num());

	AHollowExoFrameCharacter* Player = nullptr;
	if (!Scene.SpawnPlayer(*this, PlayerAt, Player) || Nest->GetBrood().Num() < 2)
	{
		return false;
	}
	AHollowWhispererController* Seer = BrainOf(Nest->GetBrood()[0].Get());
	Seer->ReportSeen(Player);
	bool bTold = true;
	for (int32 Index = 1; Index < Nest->GetBrood().Num(); ++Index)
	{
		const AHollowWhispererController* Mate = BrainOf(Nest->GetBrood()[Index].Get());
		bTold &= Mate && Mate->HasStimulus() && FVector::Dist(Mate->GetStimulusLocation(), Player->GetActorLocation()) < 1.0;
	}
	TestTrue(TEXT("One sees the player: the rest of its brood hear where"), Seer->GetTarget() == Player && bTold);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowWhispererLightTest, "Hollow.Game.Whisperer.Light",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowWhispererLightTest::RunTest(const FString& Parameters)
{
	// Clarifications 2-4: a player's lamp on a Whisperer makes it flinch with a shriek, slowed while the beam stays,
	// and back away from it; a light again too soon does not make it flinch again, later it does. The lamp off, its prey
	// without a light, it goes into a frenzy (faster, louder).
	using namespace HollowWhispererTests;
	FEnemyScene Scene;
	if (!TestNotNull(TEXT("GCN_Whisperer_Windup"), LoadClass<AActor>(nullptr, BiteCueClass)) || !Scene.Create(*this, EWall::None))
	{
		return false;
	}
	AHollowExoFrameCharacter* Player = nullptr;
	Scene.SpawnPlayer(*this, PlayerAt, Player);
	AHollowEnemyCharacter* Enemy = Scene.SpawnEnemy(*this, WhispererClass, AheadAt);
	AHollowWhisperer* Whisperer = Cast<AHollowWhisperer>(Enemy);
	AHollowWhispererController* Brain = BrainOf(Enemy);
	UHollowSwarmSubsystem* Swarm = UHollowSwarmSubsystem::Get(Scene.World);
	const UHollowWhispererDefinition* Tuning = Definition();
	if (!Player || !TestNotNull(TEXT("BP_Whisperer"), Whisperer) || !TestNotNull(TEXT("Its brain"), Brain) || !Swarm || !Tuning)
	{
		return false;
	}
	Advance(Scene.TestWorld, Settle);
	Aim(Player, Whisperer->GetActorLocation());
	Brain->ReportSeen(Player);
	TestTrue(TEXT("Its prey has a light"), Brain->PreyHasLight());

	Swarm->CheckLight();
	Advance(Scene.TestWorld, Settle);
	TestTrue(TEXT("In the lamp: lit"), Brain->IsLit() && Swarm->GetLitCount() == 1);
	TestTrue(TEXT("...it flinches"), Brain->GetActivity() == EHollowAIActivity::Recoil && Brain->GetRecoilCount() == 1);
	TestTrue(TEXT("...with a shriek, its eyes flaring"), Whisperer->GetShriekCount() == 1);
	TestTrue(FString::Printf(TEXT("...slowed (x%.2f)"), Brain->GetMoveSpeedMultiplier()), FMath::IsNearlyEqual(Brain->GetMoveSpeedMultiplier(), Multiplier(Tuning->LitEffect), SpeedTolerance));
	FVector LightOrigin;
	TestTrue(TEXT("...it knows where the light came from"), Brain->GetLastLight(LightOrigin) && FVector::Dist(LightOrigin, Player->GetPawnViewLocation()) < 1.0);
	const double Start = FVector::Dist2D(Whisperer->GetActorLocation(), Player->GetActorLocation());
	double Farthest = Start;
	Advance(Scene.TestWorld, Tuning->FlinchSeconds + 1.2f, [&] {
		Farthest = FMath::Max(Farthest, FVector::Dist2D(Whisperer->GetActorLocation(), Player->GetActorLocation()));
		return false;
	});
	TestTrue(FString::Printf(TEXT("...and backs away from it (%.0f -> %.0f cm)"), Start, Farthest), Farthest > Start + Tuning->RecoilDistance * 0.5);

	// A light again too soon: no second flinch; later, again.
	UAbilitySystemComponent* Abilities = Whisperer->GetAbilitySystemComponent();
	Player->SetHelmetLampOn(false);
	Abilities->RemoveActiveGameplayEffectBySourceEffect(Tuning->LitEffect, nullptr);
	Advance(Scene.TestWorld, Settle);
	Abilities->ApplyGameplayEffectToSelf(Tuning->LitEffect.GetDefaultObject(), 1.f, Abilities->MakeEffectContext());
	Advance(Scene.TestWorld, Settle);
	TestEqual(TEXT("Lit again within its cooldown: no second flinch"), Brain->GetRecoilCount(), 1);
	Abilities->RemoveActiveGameplayEffectBySourceEffect(Tuning->LitEffect, nullptr);
	Advance(Scene.TestWorld, Tuning->RecoilCooldownSeconds);
	Abilities->ApplyGameplayEffectToSelf(Tuning->LitEffect.GetDefaultObject(), 1.f, Abilities->MakeEffectContext());
	Advance(Scene.TestWorld, Settle);
	TestEqual(TEXT("...after it: it flinches again"), Brain->GetRecoilCount(), 2);
	Abilities->RemoveActiveGameplayEffectBySourceEffect(Tuning->LitEffect, nullptr);

	// The lamp off: no light anywhere, its prey in the dark.
	const UHollowThreatConfig* Threat = UHollowThreatConfig::Get();
	Advance(Scene.TestWorld, Threat->Swarm.LightCheckSeconds * 2.f + Settle);
	TestFalse(TEXT("The lamp off: not lit"), Brain->IsLit());
	TestTrue(TEXT("...its prey in the dark"), Brain->GetTarget() == Player && !Brain->PreyHasLight());
	TestTrue(TEXT("...a frenzy"), Brain->IsFrenzied() && Swarm->GetFrenziedCount() == 1);
	TestTrue(FString::Printf(TEXT("...faster (x%.2f)"), Brain->GetMoveSpeedMultiplier()), FMath::IsNearlyEqual(Brain->GetMoveSpeedMultiplier(), Multiplier(Tuning->FrenzyEffect), SpeedTolerance));
	TestTrue(TEXT("...and it may bite whatever the others do"), Brain->CanBite());
	Player->SetHelmetLampOn(true);
	Aim(Player, Whisperer->GetActorLocation());
	Swarm->CheckLight();
	TestFalse(TEXT("The lamp back on it: no frenzy renewed"), Swarm->GetFrenziedCount() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowWhispererFlankTest, "Hollow.Game.Whisperer.Flank",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowWhispererFlankTest::RunTest(const FString& Parameters)
{
	// A Whisperer off to the side of a player with a lamp goes round through the dark (EQ_WhispererFlank: out of every
	// light) and bites from there, never once in the beam. At most two bite a lit player; a frenzied third bites too.
	using namespace HollowWhispererTests;
	FEnemyScene Scene;
	if (!TestNotNull(TEXT("GCN_Whisperer_Windup"), LoadClass<AActor>(nullptr, BiteCueClass)) || !Scene.Create(*this, EWall::None))
	{
		return false;
	}
	AHollowExoFrameCharacter* Player = nullptr;
	UHollowAbilitySystemComponent* PlayerAbilities = Scene.SpawnPlayer(*this, PlayerAt, Player);
	AHollowEnemyCharacter* Flanker = Scene.SpawnEnemy(*this, WhispererClass, AsideAt);
	AHollowWhispererController* Brain = BrainOf(Flanker);
	UHollowSwarmSubsystem* Swarm = UHollowSwarmSubsystem::Get(Scene.World);
	const UHollowWhispererDefinition* Tuning = Definition();
	if (!PlayerAbilities || !TestNotNull(TEXT("Its brain"), Brain) || !Swarm || !Tuning)
	{
		return false;
	}
	Aim(Player, AheadAt);   // the lamp looks away from it
	Advance(Scene.TestWorld, Settle);
	Brain->ReportSeen(Player);
	TestTrue(TEXT("Its prey has a light: it flanks"), Brain->ShouldFlank());
	const float Health = HollowEnemyTests::Health(PlayerAbilities);
	bool bEverLit = false;
	bool bFlanked = false;
	TArray<FHollowLightCone> Cones;
	Advance(Scene.TestWorld, 12.f, [&] {
		bEverLit |= Brain->IsLit();
		bFlanked |= Brain->GetActivity() == EHollowAIActivity::Flank;
		return HollowEnemyTests::Health(PlayerAbilities) < Health;
	});
	TestTrue(TEXT("...it went round"), bFlanked);
	TestTrue(TEXT("...and bit"), HollowEnemyTests::Health(PlayerAbilities) < Health);
	TestFalse(TEXT("...never once in the beam"), bEverLit);
	Player->GetLightCones(Cones);
	TestTrue(TEXT("...the lamp still on, ahead"), Cones.Num() == 1 && !Cones[0].Contains(Flanker->GetActorLocation()));

	// Bite slots.
	AHollowEnemyCharacter* Second = Scene.SpawnEnemy(*this, WhispererClass, AsideAt + FVector(200.0, 0.0, 0.0));
	AHollowEnemyCharacter* Third = Scene.SpawnEnemy(*this, WhispererClass, AsideAt + FVector(400.0, 0.0, 0.0));
	AHollowWhispererController* SecondBrain = BrainOf(Second);
	AHollowWhispererController* ThirdBrain = BrainOf(Third);
	if (!SecondBrain || !ThirdBrain)
	{
		return false;
	}
	for (AHollowWhispererController* Each : {Brain, SecondBrain, ThirdBrain})
	{
		Each->GetBrain()->StopLogic(TEXT("Hollow.Game.Whisperer.Flank: slots by hand"));
		Each->ReportSeen(Player);
	}
	Swarm->ReleaseBiteSlot(*Brain);
	Swarm->TakeBiteSlot(*Brain, Player);
	Swarm->TakeBiteSlot(*SecondBrain, Player);
	TestEqual(TEXT("Two bite the lit player"), Swarm->CountBiters(Player), 2);
	TestTrue(TEXT("...they keep their turns"), Brain->CanBite() && SecondBrain->CanBite());
	TestFalse(TEXT("...the third waits its turn"), ThirdBrain->CanBite());
	TestTrue(TEXT("...in the dark"), ThirdBrain->ShouldFlank());
	UAbilitySystemComponent* ThirdAbilities = Third->GetAbilitySystemComponent();
	ThirdAbilities->ApplyGameplayEffectToSelf(Tuning->FrenzyEffect.GetDefaultObject(), 1.f, ThirdAbilities->MakeEffectContext());
	TestTrue(TEXT("A frenzied one bites anyway"), ThirdBrain->CanBite());
	Swarm->ReleaseBiteSlot(*Brain);
	TestEqual(TEXT("A bite over: its slot free"), Swarm->CountBiters(Player), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowWhispererLanternTest, "Hollow.Game.Whisperer.Lantern",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowWhispererLanternTest::RunTest(const FString& Parameters)
{
	// Clarifications 3 and 8: the Aether lantern's beam is a light the AI knows (IHollowLightSource, Kind Lantern)
	// while it burns; on a Whisperer it exposes (a hit is a crit), it lights and it flees far.
	using namespace HollowWhispererTests;
	FEnemyScene Scene;
	const TSubclassOf<UGameplayAbility> LanternClass = LoadClass<UGameplayAbility>(nullptr, LanternAbilityClass);
	const TSubclassOf<UGameplayEffect> Kinetic = LoadClass<UGameplayEffect>(nullptr, KineticEffect);
	if (!TestNotNull(TEXT("GA_Tool_AetherLantern"), LanternClass.Get()) || !TestNotNull(TEXT("GE_Damage_Kinetic"), Kinetic.Get())
		|| !TestNotNull(TEXT("GCN_Tool_Lantern"), LoadClass<AActor>(nullptr, LanternCueClass)) || !TestNotNull(TEXT("GCN_Status_Exposed"), LoadClass<AActor>(nullptr, ExposedCueClass))
		|| !TestNotNull(TEXT("GCN_Whisperer_Windup"), LoadClass<AActor>(nullptr, BiteCueClass)) || !Scene.Create(*this, EWall::None))
	{
		return false;
	}
	AHollowExoFrameCharacter* Player = nullptr;
	UHollowAbilitySystemComponent* Abilities = Scene.SpawnPlayer(*this, PlayerAt, Player, 0.f, /*bLocalPlayer=*/true);
	AHollowEnemyCharacter* Whisperer = Scene.SpawnEnemy(*this, WhispererClass, AheadAt);
	AHollowWhispererController* Brain = BrainOf(Whisperer);
	UHollowSwarmSubsystem* Swarm = UHollowSwarmSubsystem::Get(Scene.World);
	const UHollowWhispererDefinition* Tuning = Definition();
	if (!Abilities || !TestNotNull(TEXT("Its brain"), Brain) || !Swarm || !Tuning)
	{
		return false;
	}
	Advance(Scene.TestWorld, Settle);
	Player->SetHelmetLampOn(false);   // the lantern alone
	Aim(Player, Whisperer->GetActorLocation());
	FGameplayAbilitySpec Spec(LanternClass, 1);
	Spec.GetDynamicSpecSourceTags().AddTag(HollowTags::Input_PrimaryUse);
	Abilities->GiveAbility(Spec);
	TArray<FHollowLightCone> Cones;
	Player->GetLightCones(Cones);
	TestEqual(TEXT("Lamp off, lantern unlit: no light"), Cones.Num(), 0);

	Abilities->AbilityInputTagPressed(HollowTags::Input_PrimaryUse);
	Cones.Reset();
	Player->GetLightCones(Cones);
	TestTrue(TEXT("The lantern burns: its beam is a light"), Cones.Num() == 1 && Cones[0].Kind == EHollowLightKind::Lantern);
	const double Start = FVector::Dist2D(Whisperer->GetActorLocation(), Player->GetActorLocation());
	Swarm->CheckLight();
	Advance(Scene.TestWorld, 0.35f);
	TestTrue(TEXT("...on the Whisperer: exposed and lit"), Has(Whisperer, HollowTags::State_Exposed) && Brain->IsLit());
	TestEqual(TEXT("...it flinches once"), Brain->GetRecoilCount(), 1);
	TestEqual(TEXT("A hit now is a crit"), HollowHitMarker::KindOf(Whisperer, Kinetic, 20.f, 0.1f, 0.2f), EHollowHitKind::Crit);
	Abilities->AbilityInputTagReleased(HollowTags::Input_PrimaryUse);
	double Farthest = Start;
	Advance(Scene.TestWorld, Tuning->FlinchSeconds + 2.5f, [&] {
		Farthest = FMath::Max(Farthest, FVector::Dist2D(Whisperer->GetActorLocation(), Player->GetActorLocation()));
		return false;
	});
	TestTrue(FString::Printf(TEXT("...and flees farther than from a lamp (%.0f -> %.0f cm)"), Start, Farthest), Farthest > Start + Tuning->RecoilDistance);
	Cones.Reset();
	Player->GetLightCones(Cones);
	TestEqual(TEXT("Released: no beam"), Cones.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowWhispererKillTest, "Hollow.Game.Whisperer.Kill",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowWhispererKillTest::RunTest(const FString& Parameters)
{
	// GP-02 brief ("can and must be killed"): three nailgun hits kill one, a hit on it is a plain hit (no armour); dead, it stops
	// whispering and dies with a sound; one whose seed rolls its drop leaves an Aether shard (found loot), one whose seed
	// does not leaves nothing.
	using namespace HollowWhispererTests;
	FEnemyScene Scene;
	const TSubclassOf<UGameplayEffect> Kinetic = LoadClass<UGameplayEffect>(nullptr, KineticEffect);
	const UHollowWhispererDefinition* Tuning = Definition();
	if (!TestNotNull(TEXT("GE_Damage_Kinetic"), Kinetic.Get()) || !Tuning || !TestNotNull(TEXT("GCN_Whisperer_Windup"), LoadClass<AActor>(nullptr, BiteCueClass))
		|| !Scene.Create(*this, EWall::None))
	{
		return false;
	}
	// The first seed that rolls the drop and the first that does not.
	int32 Rolls = INDEX_NONE;
	int32 Misses = INDEX_NONE;
	for (int32 Seed = 0; Seed < Seeds && (Rolls == INDEX_NONE || Misses == INDEX_NONE); ++Seed)
	{
		int32& Found = Tuning->Drop.Rolls(Seed) ? Rolls : Misses;
		Found = Found == INDEX_NONE ? Seed : Found;
	}
	AHollowExoFrameCharacter* Player = nullptr;
	UHollowAbilitySystemComponent* PlayerAbilities = Scene.SpawnPlayer(*this, PlayerAt, Player);
	AHollowEnemyCharacter* Lucky = Scene.SpawnEnemy(*this, WhispererClass, AheadAt);
	AHollowEnemyCharacter* Unlucky = Scene.SpawnEnemy(*this, WhispererClass, AsideAt);
	const UHollowEnemyDropSubsystem* Drops = UHollowEnemyDropSubsystem::Get(Scene.World);
	const UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(Scene.World);
	if (!PlayerAbilities || !Lucky || !Unlucky || !TestNotNull(TEXT("Drop subsystem"), Drops) || !TestTrue(TEXT("Seeds that roll and miss"), Rolls != INDEX_NONE && Misses != INDEX_NONE))
	{
		return false;
	}
	Lucky->SetSpawnSeed(Rolls);
	Unlucky->SetSpawnSeed(Misses);
	Advance(Scene.TestWorld, Settle);
	TestEqual(TEXT("A hit on it: a plain hit"), HollowHitMarker::KindOf(Lucky, Kinetic, 20.f, 0.1f, 0.2f), EHollowHitKind::Hit);
	constexpr float NailDamage = 20.f;
	ApplyFrom(PlayerAbilities, Lucky, Kinetic, NailDamage);
	ApplyFrom(PlayerAbilities, Lucky, Kinetic, NailDamage);
	TestFalse(TEXT("Two nails: still alive"), Lucky->IsDead());
	const int32 DeathSounds = Audio ? Audio->GetPlayCount(HollowTags::Sound_Whisperer_Death) : 0;
	ApplyFrom(PlayerAbilities, Lucky, Kinetic, NailDamage);
	TestTrue(TEXT("Three: dead"), Lucky->IsDead());
	TestTrue(TEXT("...silent, its death heard"), !Cast<AHollowWhisperer>(Lucky)->GetWhisper() && (!Audio || Audio->GetPlayCount(HollowTags::Sound_Whisperer_Death) == DeathSounds + 1));
	TestEqual(TEXT("...it leaves its drop"), Drops->GetDropCount(), 1);
	const AHollowLootPickup* Pickup = Drops->GetLastDrop();
	TestTrue(TEXT("...an Aether shard where it died"), Pickup && Pickup->GetItem() && Pickup->GetItem()->GetPrimaryAssetId() == AetherShard
		&& Pickup->GetCount() == Tuning->Drop.Count && FVector::Dist2D(Pickup->GetActorLocation(), Lucky->GetActorLocation()) < 100.0);
	for (int32 Hit = 0; Hit < 3; ++Hit)
	{
		ApplyFrom(PlayerAbilities, Unlucky, Kinetic, NailDamage);
	}
	TestTrue(TEXT("The other dead too"), Unlucky->IsDead());
	TestEqual(TEXT("...its seed does not roll: nothing"), Drops->GetDropCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowWhispererDoorsTest, "Hollow.Game.Whisperer.Doors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowWhispererDoorsTest::RunTest(const FString& Parameters)
{
	// Clarification 1: a Whisperer after a player behind a plain closed door opens it and comes through (it breaks
	// nothing: a welded one it goes around, CanBreach as the stalker's).
	using namespace HollowWhispererTests;
	FEnemyScene Scene;
	if (!TestNotNull(TEXT("GCN_Whisperer_Windup"), LoadClass<AActor>(nullptr, BiteCueClass)) || !Scene.Create(*this, EWall::WithDoor))
	{
		return false;
	}
	AHollowExoFrameCharacter* Player = nullptr;
	Scene.SpawnPlayer(*this, RoomA, Player);
	AHollowEnemyCharacter* Whisperer = Scene.SpawnEnemy(*this, WhispererClass, FVector(1500.0, 0.0, 40.0));
	AHollowWhispererController* Brain = BrainOf(Whisperer);
	const IHollowStructural* Door = Cast<IHollowStructural>(Scene.Door);
	if (!Player || !TestNotNull(TEXT("Its brain"), Brain) || !TestNotNull(TEXT("BP_Door"), Door))
	{
		return false;
	}
	Player->SetHelmetLampOn(false);   // no light to flinch from
	TestTrue(TEXT("The door is closed"), Door->GetPassage() != EHollowPassage::Clear);
	Advance(Scene.TestWorld, Settle);
	Brain->ReportSeen(Player);
	Advance(Scene.TestWorld, 20.f, [Whisperer] { return Whisperer->GetActorLocation().X < WallX - 100.0; });
	TestTrue(TEXT("It opened the door"), Door->GetPassage() == EHollowPassage::Clear);
	TestTrue(FString::Printf(TEXT("...and came through (x %.0f)"), Whisperer->GetActorLocation().X), Whisperer->GetActorLocation().X < WallX - 100.0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
