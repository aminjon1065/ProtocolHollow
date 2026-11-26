#include "Tests/HollowEnemyTestScene.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Audio/HollowSoundBank.h"
#include "Character/HollowExoFrameCharacter.h"
#include "Framework/HollowPlayerController.h"
#include "Noise/HollowAcousticAlertSubsystem.h"
#include "Noise/HollowNoiseConfig.h"
#include "Noise/HollowNoiseSubsystem.h"
#include "Tests/HollowGameTestUtils.h"

namespace HollowAcousticAlertTests
{
	using namespace HollowEnemyTests;

	const TCHAR* const StalkerClass = TEXT("/Game/Hollow/Characters/Enemies/EchoStalker/BP_EchoStalker.BP_EchoStalker_C");
	/** Players behind the solid wall in room B: the Pilgrim in room A sees nobody, so a noise is what wakes it. */
	const FVector PlayerA(1500.0, 300.0, 100.0);
	const FVector PlayerB(1500.0, -300.0, 100.0);
	/** Each noise somewhere new in room A (farther apart than its move acceptance), so each one wakes it again. */
	const FVector Noises[] = {FVector(300.0, 300.0, 0.0), FVector(300.0, -300.0, 0.0), FVector(700.0, 300.0, 0.0), FVector(700.0, -300.0, 0.0),
		FVector(500.0, 400.0, 0.0), FVector(500.0, -400.0, 0.0)};
	constexpr float Margin = 0.5f;

	AHollowPlayerController* SpawnPlayer(FAutomationTestBase& Test, UWorld* World, const FVector& Location)
	{
		AHollowExoFrameCharacter* Character = HollowGameTests::SpawnExoFrame(Test, World, FTransform(Location));
		return Character ? HollowGameTests::PossessAsPlayer<AHollowPlayerController>(World, Character) : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowAcousticAlertTest, "Hollow.AI.AcousticAlert",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowAcousticAlertTest::RunTest(const FString& Parameters)
{
	// POLISH-01 (ADR-0053): when a player's noise wakes the Iron Pilgrim, it calls out (its acoustic alert, a sound where
	// it is, on every machine) and the player who made the noise - only that one - sees ACOUSTIC ALERT on its visor, at
	// most once per DA_NoiseConfig's cooldown. Footsteps never do; the enemy answers at most once per AnswerInterval.
	using namespace HollowAcousticAlertTests;
	FEnemyScene Scene;
	if (!Scene.Create(*this, EWall::Solid))
	{
		return false;
	}
	UWorld* World = Scene.World;
	const UHollowNoiseSubsystem* Noise = UHollowNoiseSubsystem::Get(World);
	const UHollowNoiseConfig* Config = Noise ? Noise->GetConfig() : nullptr;
	const UHollowAcousticAlertSubsystem* Alerts = UHollowAcousticAlertSubsystem::Get(World);
	AHollowEnemyCharacter* Pilgrim = Scene.SpawnPilgrim(*this, RoomA);
	AHollowAIController* Brain = Pilgrim ? Cast<AHollowAIController>(Pilgrim->GetController()) : nullptr;
	AHollowPlayerController* A = SpawnPlayer(*this, World, PlayerA);
	AHollowPlayerController* B = SpawnPlayer(*this, World, PlayerB);
	if (!TestNotNull(TEXT("DA_NoiseConfig"), Config) || !TestNotNull(TEXT("Acoustic alert subsystem"), Alerts) || !TestNotNull(TEXT("Pilgrim"), Brain)
		|| !TestNotNull(TEXT("Player A"), A) || !TestNotNull(TEXT("Player B"), B))
	{
		return false;
	}

	// The shipped data: both hunters call out, each with its own sound, heard far; the bank plays them.
	const UHollowSoundBank* Bank = UHollowSoundBank::Get();
	const TSubclassOf<AHollowEnemyCharacter> Stalker = LoadClass<AHollowEnemyCharacter>(nullptr, StalkerClass);
	const UHollowEnemyDefinition* StalkerDefinition = Stalker ? Stalker.GetDefaultObject()->GetDefinition() : nullptr;
	TestTrue(TEXT("The Pilgrim's alert"), Pilgrim->GetDefinition()->AcousticAlertSound == HollowTags::Sound_Enemy_PilgrimAlert);
	TestTrue(TEXT("The Echo-Stalker's alert"), StalkerDefinition && StalkerDefinition->AcousticAlertSound == HollowTags::Sound_Enemy_StalkerAlert);
	for (const FGameplayTag& Sound : {HollowTags::Sound_Enemy_PilgrimAlert.GetTag(), HollowTags::Sound_Enemy_StalkerAlert.GetTag()})
	{
		TestTrue(*FString::Printf(TEXT("DA_SoundBank plays %s"), *Sound.ToString()), Bank && Bank->Find(Sound) != nullptr);
	}

	Advance(Scene.TestWorld, TickSeconds * 4.f);
	TestTrue(TEXT("Nobody seen: it patrols"), Brain->GetActivity() == EHollowAIActivity::Patrol && !Brain->GetTarget());
	Brain->ReportHeard(Noises[0], A->GetPawn(), HollowTags::Noise_Footstep);
	TestEqual(TEXT("A footstep wakes it, but it does not call out"), Pilgrim->GetAnswersHeard(), 0);

	Brain->ReportHeard(Noises[1], A->GetPawn(), HollowTags::Noise_Voice_Shout);
	TestEqual(TEXT("A shout wakes it: it calls out"), Pilgrim->GetAnswersHeard(), 1);
	TestTrue(TEXT("...and the shouter's visor hears of it"), A->GetAcousticAlerts() == 1 && Alerts->GetAlertsSent() == 1);
	TestEqual(TEXT("...the other player's does not"), B->GetAcousticAlerts(), 0);

	Brain->ReportHeard(Noises[2], B->GetPawn(), HollowTags::Noise_Weapon_Nailgun);
	TestEqual(TEXT("Another noise at once: it does not call out again"), Pilgrim->GetAnswersHeard(), 1);

	Advance(Scene.TestWorld, Config->AnswerInterval + Margin);
	Brain->ReportHeard(Noises[3], B->GetPawn(), HollowTags::Noise_Weapon_Nailgun);
	TestEqual(TEXT("After its interval a shot wakes it: it calls out"), Pilgrim->GetAnswersHeard(), 2);
	TestTrue(TEXT("...and the shooter's visor hears of it"), B->GetAcousticAlerts() == 1 && A->GetAcousticAlerts() == 1);

	Advance(Scene.TestWorld, Config->AnswerInterval + Margin);
	Brain->ReportHeard(Noises[4], A->GetPawn(), HollowTags::Noise_Tool_Welder);
	const bool bCooled = Config->AnswerInterval * 2.f + Margin * 2.f >= Config->AcousticAlertCooldown;
	TestEqual(TEXT("Welding wakes it: it calls out"), Pilgrim->GetAnswersHeard(), 3);
	TestEqual(TEXT("...the welder's visor only once per cooldown"), A->GetAcousticAlerts(), bCooled ? 2 : 1);

	Advance(Scene.TestWorld, Config->AcousticAlertCooldown + Margin);
	Brain->ReportHeard(Noises[5], A->GetPawn(), HollowTags::Noise_Tool_Welder);
	TestEqual(TEXT("After the cooldown the welder's visor hears again"), A->GetAcousticAlerts(), bCooled ? 3 : 2);
	TestEqual(TEXT("The other player's visor heard only its own"), B->GetAcousticAlerts(), 1);

	// A dead one answers nothing: neither a sound nor an alert, nor any player's cooldown spent.
	const int32 Answers = Pilgrim->GetAnswersHeard();
	const int32 AlertsOfA = A->GetAcousticAlerts();
	Pilgrim->Die();
	Advance(Scene.TestWorld, Config->AcousticAlertCooldown + Margin);
	Brain->ReportHeard(Noises[0], A->GetPawn(), HollowTags::Noise_Voice_Shout);
	TestTrue(TEXT("Dead: no answer, no alert"), Pilgrim->GetAnswersHeard() == Answers && A->GetAcousticAlerts() == AlertsOfA);
	return true;
}

#endif
