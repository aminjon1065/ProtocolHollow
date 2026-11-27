#include "Tests/HollowEnemyTestScene.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/HollowEnemyAttackDefinition.h"
#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Brain/HollowAIController.h"
#include "Enemies/HollowEnemyDefinition.h"
#include "Enemies/HollowEnemyMontageComponent.h"
#include "GameFramework/Character.h"
#include "Tags/HollowGameplayTags.h"
#include "UObject/Package.h"

namespace HollowEnemyMontageTests
{
	using namespace HollowEnemyTests;

	// Content under test (create_pilgrim_assets.py, create_stalker_assets.py).
	const TCHAR* const PilgrimDefinition = TEXT("/Game/Hollow/Characters/Enemies/IronPilgrim/DA_Enemy_IronPilgrim.DA_Enemy_IronPilgrim");
	const TCHAR* const StalkerDefinition = TEXT("/Game/Hollow/Characters/Enemies/EchoStalker/DA_Enemy_EchoStalker.DA_Enemy_EchoStalker");
	const TCHAR* const BreachAttack = TEXT("/Game/Hollow/Characters/Enemies/IronPilgrim/DA_Attack_Pilgrim_Breach.DA_Attack_Pilgrim_Breach");
	const TCHAR* const AmbushAttack = TEXT("/Game/Hollow/Characters/Enemies/EchoStalker/DA_Attack_Stalker_Ambush.DA_Attack_Stalker_Ambush");
	const TCHAR* const StalkerClass = TEXT("/Game/Hollow/Characters/Enemies/EchoStalker/BP_EchoStalker.BP_EchoStalker_C");

	/** The shipped montages' slot and the stun's looping section (create_character_assets.py). */
	const FName DefaultSlot(TEXT("DefaultSlot"));
	const FName StunLoop(TEXT("Loop"));
	/** A vent's hide timer fires after its montage; ticks of slack. */
	constexpr float VentSlackSeconds = 0.2f;

	/** Test data: the blow of a montage lands 0.6 s in. */
	constexpr float ImpactAt = 0.6f;
	constexpr float Tolerance = 1.0e-4f;

	UAnimMontage* MakeMontage(const FName Impact)
	{
		UAnimMontage* Montage = NewObject<UAnimMontage>(GetTransientPackage());
		FCompositeSection Start;
		Start.SectionName = TEXT("Windup");
		Start.SetTime(0.f);
		Montage->CompositeSections.Add(Start);
		if (!Impact.IsNone())
		{
			FCompositeSection Blow;
			Blow.SectionName = Impact;
			Blow.SetTime(ImpactAt);
			Montage->CompositeSections.Add(Blow);
		}
		return Montage;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowEnemyMontagesTest, "Hollow.Game.Enemy.Montages",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowEnemyMontagesTest::RunTest(const FString& Parameters)
{
	// ADR-0027: optional montages of an enemy's moments. The shipped attacks name their slot (the breach its own); an
	// attack montage is fitted so its impact lands when the wind-up ends; CHAR-01: the shipped enemies have theirs and
	// play them; empty slots change nothing and say nothing; a montage the mesh cannot play is said once; a stalker
	// without a VentEnter montage hides at once as before.
	using namespace HollowEnemyMontageTests;
	const UHollowEnemyDefinition* Pilgrim = LoadObject<UHollowEnemyDefinition>(nullptr, PilgrimDefinition);
	const UHollowEnemyDefinition* Stalker = LoadObject<UHollowEnemyDefinition>(nullptr, StalkerDefinition);
	const UHollowEnemyAttackDefinition* Melee = LoadObject<UHollowEnemyAttackDefinition>(nullptr, MeleeAttack);
	const UHollowEnemyAttackDefinition* Breach = LoadObject<UHollowEnemyAttackDefinition>(nullptr, BreachAttack);
	const UHollowEnemyAttackDefinition* Ambush = LoadObject<UHollowEnemyAttackDefinition>(nullptr, AmbushAttack);
	if (!TestNotNull(TEXT("DA_Enemy_IronPilgrim"), Pilgrim) || !TestNotNull(TEXT("DA_Enemy_EchoStalker"), Stalker) || !TestNotNull(TEXT("Melee attack"), Melee)
		|| !TestNotNull(TEXT("Breach attack"), Breach) || !TestNotNull(TEXT("Ambush attack"), Ambush))
	{
		return false;
	}
	TestTrue(TEXT("Slots: the swing and the ambush strike are Melee, the breach is Breach"), Melee->Montage == EHollowEnemyMontage::Melee
		&& Ambush->Montage == EHollowEnemyMontage::Melee && Breach->Montage == EHollowEnemyMontage::Breach);
	TestTrue(TEXT("Both enemies fit attacks at their impact section"), !Pilgrim->ImpactSection.IsNone() && Pilgrim->ImpactSection == Stalker->ImpactSection);

	// Fitting: the impact lands at the end of the wind-up.
	const UAnimMontage* WithImpact = MakeMontage(Pilgrim->ImpactSection);
	const UAnimMontage* WithoutImpact = MakeMontage(NAME_None);
	TestNearlyEqual(TEXT("Impact at 0.6 s, wind-up 1.2 s: half speed"), HollowEnemyMontages::PlayRateToLand(*WithImpact, Pilgrim->ImpactSection, 2.f * ImpactAt), 0.5f, Tolerance);
	TestNearlyEqual(TEXT("...a very short wind-up: clamped"), HollowEnemyMontages::PlayRateToLand(*WithImpact, Pilgrim->ImpactSection, ImpactAt * 0.01f),
		HollowEnemyMontages::MaxPlayRate, Tolerance);
	TestNearlyEqual(TEXT("No impact section: as authored"), HollowEnemyMontages::PlayRateToLand(*WithoutImpact, Pilgrim->ImpactSection, 1.f), 1.f, Tolerance);
	TestNearlyEqual(TEXT("Nothing to fit: as authored"), HollowEnemyMontages::PlayRateToLand(*WithImpact, Pilgrim->ImpactSection, 0.f), 1.f, Tolerance);

	// CHAR-01 (ADR-0055): the shipped enemies' montages (create_character_assets.py): each moment they have plays in
	// their DefaultSlot with no root motion; an attack lands where its wind-up ends; the stun loops while it lasts.
	struct FShipped
	{
		const UHollowEnemyDefinition* Definition;
		TArray<EHollowEnemyMontage> Moments;
	};
	const FShipped Shipped[] = {{Pilgrim, {EHollowEnemyMontage::Melee, EHollowEnemyMontage::Breach, EHollowEnemyMontage::Stunned}},
		{Stalker, {EHollowEnemyMontage::Melee, EHollowEnemyMontage::Stunned, EHollowEnemyMontage::VentEnter, EHollowEnemyMontage::VentExit}}};
	for (const FShipped& Enemy : Shipped)
	{
		for (const EHollowEnemyMontage Moment : Enemy.Moments)
		{
			const UAnimMontage* Montage = Enemy.Definition->Montages.Get(Moment).LoadSynchronous();
			const FString What = FString::Printf(TEXT("%s %s"), *Enemy.Definition->GetName(), *UEnum::GetValueAsString(Moment));
			if (!TestNotNull(What, Montage))
			{
				continue;
			}
			TestTrue(What + TEXT(": in the DefaultSlot"), Montage->SlotAnimTracks.Num() == 1 && Montage->SlotAnimTracks[0].SlotName == DefaultSlot);
			for (const FAnimSegment& Segment : Montage->SlotAnimTracks[0].AnimTrack.AnimSegments)
			{
				const UAnimSequence* Sequence = Cast<UAnimSequence>(Segment.GetAnimReference());
				TestTrue(What + TEXT(": no root motion (the server moves the body)"), Sequence && !Sequence->bEnableRootMotion);
			}
		}
		const UAnimMontage* Stun = Enemy.Definition->Montages.Stunned.LoadSynchronous();
		const int32 Loop = Stun ? Stun->GetSectionIndex(StunLoop) : INDEX_NONE;
		TestTrue(Enemy.Definition->GetName() + TEXT(": the stun loops"), Loop != INDEX_NONE && Stun->CompositeSections[Loop].NextSectionName == StunLoop);
	}
	struct FFitted
	{
		const UHollowEnemyDefinition* Definition;
		EHollowEnemyMontage Moment;
		const UHollowEnemyAttackDefinition* Attack;
	};
	for (const FFitted& Fitted : {FFitted{Pilgrim, EHollowEnemyMontage::Melee, Melee}, FFitted{Pilgrim, EHollowEnemyMontage::Breach, Breach},
		FFitted{Stalker, EHollowEnemyMontage::Melee, Ambush}})
	{
		const UAnimMontage* Montage = Fitted.Definition->Montages.Get(Fitted.Moment).LoadSynchronous();
		const float Rate = Montage ? HollowEnemyMontages::PlayRateToLand(*Montage, Fitted.Definition->ImpactSection, Fitted.Attack->WindupSeconds) : 0.f;
		TestTrue(FString::Printf(TEXT("%s: its blow lands as the wind-up ends (x%.2f, not clamped)"), *Fitted.Attack->GetName(), Rate), Montage
			&& Montage->GetSectionIndex(Fitted.Definition->ImpactSection) != INDEX_NONE && Rate > HollowEnemyMontages::MinPlayRate
			&& Rate < HollowEnemyMontages::MaxPlayRate);
	}

	FEnemyScene Scene;
	if (!Scene.Create(*this, EWall::None))
	{
		return false;
	}
	// Played on the shipped enemies (their animation blueprints have the slot): the swing, the stun while it lasts, the
	// way into a vent (the body hides once it has played).
	AHollowEnemyCharacter* Iron = Scene.SpawnPilgrim(*this, FVector(300.0, 0.0, 150.0));
	AHollowEnemyCharacter* Echo = Scene.SpawnEnemy(*this, StalkerClass, FVector(900.0, 0.0, 150.0));
	if (!Iron || !Echo || !TestNotNull(TEXT("Pilgrim montages"), Iron->GetMontages()))
	{
		return false;
	}
	Cast<AHollowAIController>(Iron->GetController())->Shutdown();
	Cast<AHollowAIController>(Echo->GetController())->Shutdown();
	FlushAsyncLoading();   // the montages load at begin play
	UHollowEnemyMontageComponent* IronMontages = Iron->GetMontages();
	TestTrue(TEXT("The swing plays"), IronMontages->Play(EHollowEnemyMontage::Melee, Melee->WindupSeconds) > 0.f
		&& IronMontages->GetLastPlayed() == EHollowEnemyMontage::Melee);
	UHollowAbilitySystemComponent* IronAbilities = Cast<UHollowAbilitySystemComponent>(Iron->GetAbilitySystemComponent());
	UAnimInstance* IronAnimation = Iron->GetMesh()->GetAnimInstance();
	const UAnimMontage* IronStun = Pilgrim->Montages.Stunned.Get();
	IronAbilities->AddLooseGameplayTag(HollowTags::State_Stunned);
	TestTrue(TEXT("Stunned: its stun plays"), IronMontages->GetLastPlayed() == EHollowEnemyMontage::Stunned && IronAnimation
		&& IronAnimation->Montage_IsPlaying(IronStun));
	IronAbilities->RemoveLooseGameplayTag(HollowTags::State_Stunned);
	TestTrue(TEXT("...and stops when it recovers"), IronAnimation && !IronAnimation->Montage_IsPlaying(IronStun));
	const UAnimMontage* VentEnter = Stalker->Montages.VentEnter.Get();
	Echo->SetInVent(true);
	TestTrue(TEXT("Into a vent: seen going in, without collision"), !Echo->IsHidden() && !Echo->GetActorEnableCollision());
	Advance(Scene.TestWorld, (VentEnter ? VentEnter->GetPlayLength() : 0.f) + VentSlackSeconds);
	TestTrue(TEXT("...hidden once it has played"), Echo->IsHidden());
	Echo->SetInVent(false);
	TestTrue(TEXT("...and out"), !Echo->IsHidden());

	// Empty slots (the greybox): nothing plays, nothing is said (a strict run fails on any warning); into a vent at once.
	UHollowEnemyDefinition* Greybox = DuplicateObject(Stalker, GetTransientPackage());
	Greybox->Montages = FHollowEnemyMontages();
	UHollowEnemyMontageComponent* EchoMontages = Echo->GetMontages();
	EchoMontages->SetDefinition(Greybox);
	const int32 Before = EchoMontages->GetPlays();
	TestEqual(TEXT("Empty slot: no montage"), EchoMontages->Play(EHollowEnemyMontage::Melee, Ambush->WindupSeconds), 0.f);
	Echo->SetInVent(true);
	TestTrue(TEXT("No VentEnter montage: into the vent at once, as before"), Echo->IsHidden() && !Echo->GetActorEnableCollision()
		&& EchoMontages->GetPlays() == Before);
	Echo->SetInVent(false);

	// A montage its mesh cannot play (no animation instance): said once, nothing else happens.
	UHollowEnemyDefinition* WithMontage = DuplicateObject(Pilgrim, GetTransientPackage());
	WithMontage->Montages.Melee = MakeMontage(Pilgrim->ImpactSection);
	ACharacter* Bare = Scene.World->SpawnActor<ACharacter>(FVector(0.0, 600.0, 150.0), FRotator::ZeroRotator);
	UHollowEnemyMontageComponent* BareMontages = NewObject<UHollowEnemyMontageComponent>(Bare);
	BareMontages->SetDefinition(WithMontage);
	BareMontages->RegisterComponent();
	AddExpectedMessage(TEXT("but its mesh has no animation instance"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, /*Occurrences=*/1,
		/*IsRegex=*/false);
	TestEqual(TEXT("Cannot play: 0 s"), BareMontages->Play(EHollowEnemyMontage::Melee, Melee->WindupSeconds), 0.f);
	TestEqual(TEXT("...and once only"), BareMontages->Play(EHollowEnemyMontage::Melee, Melee->WindupSeconds), 0.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
