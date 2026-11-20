#include "Tests/HollowGameTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Attributes/HollowHealthSet.h"
#include "Carry/HollowAetherCore.h"
#include "Carry/HollowCarrierComponent.h"
#include "Carry/HollowCarryableComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "Raid/HollowExtractionZone.h"
#include "Raid/HollowRaidRules.h"
#include "Tags/HollowGameplayTags.h"

namespace HollowRaidGameTests
{
	// Content under test (create_raid_flow_assets.py, create_carry_assets.py, create_gas_assets.py).
	const TCHAR* const ZoneClass = TEXT("/Game/Hollow/World/Raid/BP_ExtractionZone.BP_ExtractionZone_C");
	const TCHAR* const CoreClass = TEXT("/Game/Hollow/Items/AetherCore/BP_AetherCore.BP_AetherCore_C");
	const TCHAR* const InitEffectClass = TEXT("/Game/Hollow/GAS/Effects/GE_ExoFrame_InitAttributes.GE_ExoFrame_InitAttributes_C");
	const TCHAR* const DamageEffectClass = TEXT("/Game/Hollow/GAS/Effects/GE_Damage_Generic.GE_Damage_Generic_C");
	const TCHAR* const DeadEffectClass = TEXT("/Game/Hollow/GAS/Effects/GE_State_Dead.GE_State_Dead_C");

	/** Test data. */
	constexpr double Start = 100.0;
	constexpr double Countdown = 15.0;
	constexpr double Outside = 2000.0;
	constexpr double CoreInside = 150.0;
	constexpr float HalfOfMaxHealth = 0.5f;

	/** Init attributes (real GE) on the player state's ability system of a possessed ExoFrame. */
	UHollowAbilitySystemComponent* MakePlayer(UWorld* World, AHollowExoFrameCharacter* Character)
	{
		HollowGameTests::PossessAsPlayer(World, Character);
		Character->DispatchBeginPlay();
		UHollowAbilitySystemComponent* AbilitySystem = Cast<UHollowAbilitySystemComponent>(Character->GetAbilitySystemComponent());
		const TSubclassOf<UGameplayEffect> Init = LoadClass<UGameplayEffect>(nullptr, InitEffectClass);
		if (AbilitySystem && Init)
		{
			AbilitySystem->ApplyGameplayEffectToSelf(Init.GetDefaultObject(), 1.f, AbilitySystem->MakeEffectContext());
		}
		return AbilitySystem;
	}

	AHollowAetherCore* SpawnCore(UWorld* World, const FVector& Location)
	{
		const TSubclassOf<AHollowAetherCore> Core = LoadClass<AHollowAetherCore>(nullptr, CoreClass);
		AHollowAetherCore* Actor = Core ? World->SpawnActor<AHollowAetherCore>(Core, FTransform(Location)) : nullptr;
		if (Actor)
		{
			Actor->DispatchBeginPlay();
		}
		return Actor;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowRaidRulesTest, "Hollow.Game.Raid.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowRaidRulesTest::RunTest(const FString& Parameters)
{
	using namespace HollowRaidGameTests;
	using namespace HollowRaidRules;
	const double Departure = Start + Countdown;
	TestTrue(TEXT("Nobody in the zone: nothing"), StepCountdown(false, Start, 0.0, 0) == EHollowCountdownStep::None);
	TestTrue(TEXT("Someone alive gets in: the countdown starts"), StepCountdown(false, Start, 0.0, 1) == EHollowCountdownStep::Start);
	TestTrue(TEXT("Counting"), StepCountdown(true, Start + 1.0, Departure, 1) == EHollowCountdownStep::None);
	TestTrue(TEXT("The zone empties: reset"), StepCountdown(true, Start + 1.0, Departure, 0) == EHollowCountdownStep::Reset);
	TestTrue(TEXT("Departure at the end, inclusively"), StepCountdown(true, Departure, Departure, 2) == EHollowCountdownStep::Depart);
	TestTrue(TEXT("An empty zone at departure time resets, nobody leaves"), StepCountdown(true, Departure, Departure, 0) == EHollowCountdownStep::Reset);

	TestTrue(TEXT("Alive in the zone: extracted"), OutcomeAtDeparture(EHollowRaidOutcome::None, true) == EHollowRaidOutcome::Extracted);
	TestTrue(TEXT("Alive outside: left behind"), OutcomeAtDeparture(EHollowRaidOutcome::None, false) == EHollowRaidOutcome::MissingInAction);
	TestTrue(TEXT("Dead stays dead, even in the zone"), OutcomeAtDeparture(EHollowRaidOutcome::Dead, true) == EHollowRaidOutcome::Dead);
	TestTrue(TEXT("Gone stays gone"), OutcomeAtDeparture(EHollowRaidOutcome::Abandoned, true) == EHollowRaidOutcome::Abandoned);
	TestTrue(TEXT("Over when nobody is alive"), IsOver(0) && !IsOver(1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowRaidExtractionZoneTest, "Hollow.Game.Raid.ExtractionZone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowRaidExtractionZoneTest::RunTest(const FString& Parameters)
{
	// The zone answers with an explicit query: pawns inside, free and carried cores inside, nothing outside.
	using namespace HollowRaidGameTests;
	FTestWorldWrapper TestWorld;
	UWorld* World = HollowGameTests::CreateWorld(*this, TestWorld);
	const TSubclassOf<AHollowExtractionZone> Zone = LoadClass<AHollowExtractionZone>(nullptr, ZoneClass);
	if (!World || !TestNotNull(TEXT("BP_ExtractionZone"), Zone.Get()))
	{
		return false;
	}
	AHollowExtractionZone* Extraction = World->SpawnActor<AHollowExtractionZone>(Zone, FTransform(FVector::ZeroVector));
	AHollowExoFrameCharacter* Aboard = HollowGameTests::SpawnExoFrame(*this, World, FTransform(FVector::ZeroVector));
	AHollowExoFrameCharacter* Behind = HollowGameTests::SpawnExoFrame(*this, World, FTransform(FVector(Outside, 0.0, 0.0)));
	MakePlayer(World, Aboard);
	MakePlayer(World, Behind);
	AHollowAetherCore* FreeInside = SpawnCore(World, FVector(-CoreInside, 0.0, 0.0));
	AHollowAetherCore* Carried = SpawnCore(World, FVector(Outside, CoreInside, 0.0));
	AHollowAetherCore* FreeOutside = SpawnCore(World, FVector(Outside, -CoreInside, 0.0));
	if (!TestTrue(TEXT("Cores spawned"), FreeInside && Carried && FreeOutside))
	{
		return false;
	}
	Carried->Interact(Aboard);   // the server side of Interact: the carrier in the zone lifts a core lying outside it
	TestTrue(TEXT("Carried into the zone"), Carried->GetCarryable()->GetCarryState().IsCarriedBy(Aboard));

	TArray<APawn*> Pawns;
	TArray<UHollowCarryableComponent*> Carryables;
	Extraction->Sample(Pawns, Carryables);
	TestTrue(TEXT("The player in the zone is found"), Pawns.Contains(Aboard));
	TestFalse(TEXT("The player outside is not"), Pawns.Contains(Behind));
	TestTrue(TEXT("A free core in the zone counts"), Carryables.Contains(FreeInside->GetCarryable()));
	TestTrue(TEXT("A carried core counts where its body rides"), Carryables.Contains(Carried->GetCarryable()));
	TestFalse(TEXT("A core outside does not"), Carryables.Contains(FreeOutside->GetCarryable()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowRaidDeathTest, "Hollow.Game.Raid.Death",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowRaidDeathTest::RunTest(const FString& Parameters)
{
	// Damage through GAS to zero: dead (replicated flag and state tag), abilities blocked, the carried core dropped,
	// movement stopped, the body no longer blocks the living.
	using namespace HollowRaidGameTests;
	FTestWorldWrapper TestWorld;
	UWorld* World = HollowGameTests::CreateWorld(*this, TestWorld);
	AHollowExoFrameCharacter* Character = HollowGameTests::SpawnExoFrame(*this, World);
	const TSubclassOf<UGameplayEffect> Damage = LoadClass<UGameplayEffect>(nullptr, DamageEffectClass);
	const TSubclassOf<UGameplayEffect> Dead = LoadClass<UGameplayEffect>(nullptr, DeadEffectClass);
	FClassProperty* DeathEffect = FindFProperty<FClassProperty>(AHollowExoFrameCharacter::StaticClass(), TEXT("DeathEffect"));
	if (!World || !TestNotNull(TEXT("ExoFrame"), Character) || !TestNotNull(TEXT("GE_Damage_Generic"), Damage.Get())
		|| !TestNotNull(TEXT("GE_State_Dead"), Dead.Get()) || !TestNotNull(TEXT("DeathEffect property"), DeathEffect))
	{
		return false;
	}
	DeathEffect->SetObjectPropertyValue_InContainer(Character, Dead.Get());   // BP_ExoFrame data on the bare class
	UHollowAbilitySystemComponent* AbilitySystem = MakePlayer(World, Character);
	AHollowAetherCore* Core = SpawnCore(World, FVector(CoreInside, 0.0, 0.0));
	if (!TestNotNull(TEXT("Ability system"), AbilitySystem) || !TestNotNull(TEXT("Core"), Core))
	{
		return false;
	}
	Core->Interact(Character);
	TestTrue(TEXT("Carrying before the blow"), Character->GetCarrierComponent()->IsCarrying());

	const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(Damage, 1.f, AbilitySystem->MakeEffectContext());
	const float MaxHealth = AbilitySystem->GetNumericAttribute(UHollowHealthSet::GetMaxHealthAttribute());
	Spec.Data->SetSetByCallerMagnitude(HollowTags::Data_Damage, MaxHealth * HalfOfMaxHealth);
	AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data);
	TestFalse(TEXT("Half the health: alive"), Character->IsDead());
	AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data);
	TestTrue(TEXT("Zero health: dead"), Character->IsDead());
	TestEqual(TEXT("Health at zero"), AbilitySystem->GetNumericAttribute(UHollowHealthSet::GetHealthAttribute()), 0.f);
	TestTrue(TEXT("Dead state tag"), AbilitySystem->HasMatchingGameplayTag(HollowTags::State_Dead));
	TestTrue(TEXT("Tools blocked"), AbilitySystem->AreAbilityTagsBlocked(FGameplayTagContainer(HollowTags::Ability_Tool_Welder)));
	TestFalse(TEXT("The core dropped where the carrier fell"), Core->GetCarryable()->GetCarryState().IsCarried());
	TestTrue(TEXT("No more movement"), Character->GetCharacterMovement()->MovementMode == MOVE_None);
	TestTrue(TEXT("The body lets the living pass"), Character->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Ignore);

	AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data);
	TestEqual(TEXT("Dying once: a second blow adds no second death effect"), AbilitySystem->GetGameplayEffectCount(Dead, nullptr), 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
