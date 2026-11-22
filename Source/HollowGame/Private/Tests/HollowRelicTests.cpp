#include "Tests/HollowGameTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/HollowAbilitySet.h"
#include "Algo/AnyOf.h"
#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Attributes/HollowGearSet.h"
#include "Attributes/HollowResonanceSet.h"
#include "Character/HollowFootstepNoiseComponent.h"
#include "Classes/HollowClassDefinition.h"
#include "Components/StaticMeshComponent.h"
#include "Crafting/HollowCraftingRules.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "Inventory/HollowInventoryComponent.h"
#include "Items/HollowItemDefinition.h"
#include "Movement/HollowCharacterMovementComponent.h"
#include "Movement/HollowMovementTuning.h"
#include "Noise/HollowNoiseConfig.h"
#include "Noise/HollowNoiseSubsystem.h"
#include "Profile/HollowEconomySettings.h"
#include "Profile/HollowProfileTypes.h"
#include "Tags/HollowGameplayTags.h"
#include "UObject/Package.h"
#include "World/HollowToolTestTarget.h"

namespace HollowRelicTests
{
	// Content under test (create_relic_assets.py, create_economy_assets.py, create_tool_assets.py, create_gas_assets.py).
	const TCHAR* const RingPath = TEXT("/Game/Hollow/Items/Relics/DA_Item_Relic_RingOfSilence.DA_Item_Relic_RingOfSilence");
	const TCHAR* const StabilizerPath = TEXT("/Game/Hollow/Items/Relics/DA_Item_Relic_ResonanceStabilizer.DA_Item_Relic_ResonanceStabilizer");
	const TCHAR* const CapacitorPath = TEXT("/Game/Hollow/Items/Relics/DA_Item_Relic_PlasmaCapacitor.DA_Item_Relic_PlasmaCapacitor");
	const TCHAR* const BracePath = TEXT("/Game/Hollow/Items/Relics/DA_Item_Relic_KineticBrace.DA_Item_Relic_KineticBrace");
	const TCHAR* const WelderAbilityClass = TEXT("/Game/Hollow/Tools/Welder/GA_Tool_Welder.GA_Tool_Welder_C");
	const TCHAR* const InitEffectClass = TEXT("/Game/Hollow/GAS/Effects/GE_ExoFrame_InitAttributes.GE_ExoFrame_InitAttributes_C");
	const TCHAR* const BeamCueClass = TEXT("/Game/Hollow/GAS/Cues/GCN_Tool_Beam.GCN_Tool_Beam_C");   // loaded up front, not into a later test's world
	const TCHAR* const CubeMeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");
	const FPrimaryAssetId Reliquary(TEXT("HollowUpgrade"), TEXT("DA_Upgrade_Reliquary"));

	// S6-04 (ADR-0037): what each relic is meant to do; test data for the scene.
	constexpr float RingFactor = 0.7f;
	constexpr float StabilizerFactor = 0.75f;
	constexpr float CapacitorFactor = 1.2f;
	constexpr float BraceFactor = 1.15f;
	constexpr float Tolerance = 0.02f;
	constexpr float TickSeconds = 1.f / 60.f;
	constexpr float WeldSeconds = 1.f;
	constexpr float TargetDistance = 100.f;
	constexpr float TargetScale = 0.5f;
	constexpr float WalkSpeed = 400.f;
	constexpr float CarryScale = 0.45f;
	constexpr int32 RelicValue = 150;

	void Advance(FTestWorldWrapper& World, const float Seconds)
	{
		for (float Elapsed = 0.f; Elapsed < Seconds; Elapsed += TickSeconds)
		{
			++GFrameCounter;
			World.TickTestWorld(TickSeconds);
		}
	}

	TArray<const TCHAR*> RelicPaths()
	{
		return {RingPath, StabilizerPath, CapacitorPath, BracePath};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowRelicsInRaidTest, "Hollow.Game.Relics.InRaid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowRelicsInRaidTest::RunTest(const FString& Parameters)
{
	// ADR-0037, S6-04 acceptance: carried in the raid inventory, each relic does its work where the work is done: the
	// Ring of Silence's steps are 30 % quieter for the AI, the Plasma Capacitor's welder does 20 % more work a sample,
	// the Kinetic Brace carries 15 % faster (and nothing else), the Resonance Stabilizer slows the gain by a quarter.
	// Relics are never used up: their effects last the raid and go with the raid inventory (Clear, the raid's end).
	using namespace HollowRelicTests;
	FTestWorldWrapper TestWorld;
	UWorld* World = HollowGameTests::CreateWorld(*this, TestWorld);
	AHollowExoFrameCharacter* Character = HollowGameTests::SpawnExoFrame(*this, World);
	const TSubclassOf<UGameplayAbility> WelderClass = LoadClass<UGameplayAbility>(nullptr, WelderAbilityClass);
	const TSubclassOf<UGameplayEffect> InitClass = LoadClass<UGameplayEffect>(nullptr, InitEffectClass);
	UHollowItemDefinition* Ring = LoadObject<UHollowItemDefinition>(nullptr, RingPath);
	UHollowItemDefinition* Stabilizer = LoadObject<UHollowItemDefinition>(nullptr, StabilizerPath);
	UHollowItemDefinition* Capacitor = LoadObject<UHollowItemDefinition>(nullptr, CapacitorPath);
	UHollowItemDefinition* Brace = LoadObject<UHollowItemDefinition>(nullptr, BracePath);
	UHollowNoiseSubsystem* Noise = UHollowNoiseSubsystem::Get(World);
	if (!Character || !TestNotNull(TEXT("GA_Tool_Welder"), WelderClass.Get()) || !TestNotNull(TEXT("Init effect"), InitClass.Get())
		|| !TestNotNull(TEXT("GCN_Tool_Beam"), LoadClass<AActor>(nullptr, BeamCueClass)) || !TestNotNull(TEXT("Ring of Silence"), Ring)
		|| !TestNotNull(TEXT("Resonance Stabilizer"), Stabilizer) || !TestNotNull(TEXT("Plasma Capacitor"), Capacitor) || !TestNotNull(TEXT("Kinetic Brace"), Brace)
		|| !TestNotNull(TEXT("Noise subsystem"), Noise) || !TestNotNull(TEXT("DA_NoiseConfig"), Noise->GetConfig()))
	{
		return false;
	}
	AddExpectedMessage(TEXT("InputConfig is not set"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, /*Occurrences=*/0, /*IsRegex=*/false);
	APlayerController* Controller = HollowGameTests::PossessAsPlayer(World, Character, /*bLocalPlayer=*/true);
	Character->DispatchBeginPlay();
	Controller->SetControlRotation(FRotator::ZeroRotator);
	UHollowAbilitySystemComponent* AbilitySystem = Character->GetHollowAbilitySystemComponent();
	UHollowInventoryComponent* Inventory = UHollowInventoryComponent::Get(Character->GetPlayerState());
	if (!TestNotNull(TEXT("Ability system"), AbilitySystem) || !TestNotNull(TEXT("Raid inventory"), Inventory))
	{
		return false;
	}
	AbilitySystem->ApplyGameplayEffectToSelf(InitClass.GetDefaultObject(), 1.f, AbilitySystem->MakeEffectContext());

	// Ring of Silence: the step the AI hears.
	TArray<FHollowNoiseReport> Reports;
	Noise->OnNoiseReported.AddLambda([&Reports](const FHollowNoiseReport& Report) { Reports.Add(Report); });
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	UHollowFootstepNoiseComponent* Footsteps = Character->GetFootstepNoise();
	Movement->SetMovementMode(MOVE_Walking);
	Movement->Velocity = FVector(Noise->GetConfig()->FootstepReferenceSpeed, 0.0, 0.0);
	auto StepLoudness = [Footsteps, &Reports] { return Footsteps->Step() && !Reports.IsEmpty() ? Reports.Last().Loudness : 0.f; };
	const float Loud = StepLoudness();
	Inventory->AddItem(Ring, 1);
	const float Quiet = StepLoudness();
	TestTrue(FString::Printf(TEXT("Ring of Silence: a step %.3f instead of %.3f"), Quiet, Loud), Loud > 0.f && FMath::IsNearlyEqual(Quiet / Loud, RingFactor, Tolerance));
	Movement->Velocity = FVector::ZeroVector;

	// Plasma Capacitor: the welder's work per sample.
	FVector Eyes;
	FRotator View;
	Character->GetActorEyesViewPoint(Eyes, View);
	AHollowToolTestTarget* Target = World->SpawnActor<AHollowToolTestTarget>(AHollowToolTestTarget::StaticClass(), FTransform(Eyes + FVector::ForwardVector * TargetDistance));
	UStaticMeshComponent* TargetMesh = Target ? Cast<UStaticMeshComponent>(Target->GetRootComponent()) : nullptr;
	if (!TestNotNull(TEXT("Test target"), TargetMesh))
	{
		return false;
	}
	TargetMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, CubeMeshPath));
	TargetMesh->SetWorldScale3D(FVector(TargetScale));
	FGameplayAbilitySpec Spec(WelderClass, 1);
	Spec.GetDynamicSpecSourceTags().AddTag(HollowTags::Input_SecondaryUse);
	AbilitySystem->GiveAbility(Spec);
	const FGameplayTag WeldTag = HollowTags::Ability_Tool_Welder;
	auto Weld = [&]
	{
		const int32 Samples = Target->GetSamples(WeldTag);
		const float Work = Target->GetTotalWork(WeldTag);
		AbilitySystem->AbilityInputTagPressed(HollowTags::Input_SecondaryUse);
		Advance(TestWorld, WeldSeconds);
		AbilitySystem->AbilityInputTagReleased(HollowTags::Input_SecondaryUse);
		const int32 Done = Target->GetSamples(WeldTag) - Samples;
		return Done > 0 ? (Target->GetTotalWork(WeldTag) - Work) / Done : 0.f;
	};
	const float Plain = Weld();
	Inventory->AddItem(Capacitor, 1);
	const float Boosted = Weld();
	TestTrue(FString::Printf(TEXT("Plasma Capacitor: %.3f work a sample instead of %.3f"), Boosted, Plain), Plain > 0.f
		&& FMath::IsNearlyEqual(Boosted / Plain, CapacitorFactor, Tolerance));

	// Kinetic Brace: only a carry is faster.
	UHollowMovementTuning* Tuning = NewObject<UHollowMovementTuning>(GetTransientPackage());
	Tuning->Ground.WalkSpeed = WalkSpeed;
	Tuning->Ground.CrouchSpeed = WalkSpeed;
	Tuning->Carry.Solo.SpeedScale = CarryScale;
	UHollowCharacterMovementComponent* HollowMovement = Character->GetHollowMovement();
	HollowMovement->SetMovementTuning(Tuning);
	HollowMovement->SetMovementMode(MOVE_Walking);
	const float Walking = HollowMovement->GetMaxSpeed();
	HollowMovement->SetClaimedCarry(EHollowCarryClaim::Solo);
	const float Carrying = HollowMovement->GetMaxSpeed();
	Inventory->AddItem(Brace, 1);
	const float Braced = HollowMovement->GetMaxSpeed();
	TestTrue(FString::Printf(TEXT("Kinetic Brace: carrying at %.0f instead of %.0f"), Braced, Carrying), Carrying > 0.f && Carrying < Walking
		&& FMath::IsNearlyEqual(Braced / Carrying, BraceFactor, Tolerance));
	HollowMovement->SetClaimedCarry(EHollowCarryClaim::None);
	TestTrue(TEXT("...walking free is no faster"), FMath::IsNearlyEqual(HollowMovement->GetMaxSpeed(), Walking));

	// Resonance Stabilizer: the gain multiplier every Resonance gain goes through.
	const FGameplayAttribute Gain = UHollowResonanceSet::GetResonanceGainMultiplierAttribute();
	const float FullGain = AbilitySystem->GetNumericAttribute(Gain);
	Inventory->AddItem(Stabilizer, 1);
	TestTrue(TEXT("Resonance Stabilizer: the gain a quarter lower"), FMath::IsNearlyEqual(AbilitySystem->GetNumericAttribute(Gain), FullGain * StabilizerFactor, Tolerance));

	// The raid over: the raid inventory goes, and every relic's effect with it.
	Inventory->Clear();
	TestTrue(TEXT("After the raid: the gear neutral, the gain as before"), FMath::IsNearlyEqual(AbilitySystem->GetNumericAttribute(UHollowGearSet::GetFootstepNoiseMultiplierAttribute()), 1.f)
		&& FMath::IsNearlyEqual(AbilitySystem->GetNumericAttribute(UHollowGearSet::GetToolWorkMultiplierAttribute()), 1.f)
		&& FMath::IsNearlyEqual(AbilitySystem->GetNumericAttribute(UHollowGearSet::GetCarrySpeedMultiplierAttribute()), 1.f)
		&& FMath::IsNearlyEqual(AbilitySystem->GetNumericAttribute(Gain), FullGain));
	Movement->SetMovementMode(MOVE_Walking);
	Movement->Velocity = FVector(Noise->GetConfig()->FootstepReferenceSpeed, 0.0, 0.0);
	TestTrue(TEXT("...a step as loud as before"), FMath::IsNearlyEqual(StepLoudness(), Loud, Loud * Tolerance));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowRelicsDataTest, "Hollow.Game.Relics.Data",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowRelicsDataTest::RunTest(const FString& Parameters)
{
	// The shipped relics and slots (create_relic_assets.py, create_economy_assets.py, DefaultGame.ini): four relics, each
	// a relic in the relic slot, insurable, one a slot, its set granting one lasting effect; each made on the workbench
	// and not sold in the shop. A loadout holds one tool a hand, one gadget, one relic; the Reliquary adds a relic slot.
	// The raid items know their slots, and every class's starting loadout keeps to them.
	using namespace HollowRelicTests;
	const FHollowEconomyTerms Terms = FHollowEconomyTerms::Load(FHollowProfileState());
	for (const TCHAR* Path : RelicPaths())
	{
		const UHollowItemDefinition* Relic = LoadObject<UHollowItemDefinition>(nullptr, Path);
		if (!TestNotNull(Path, Relic))
		{
			continue;
		}
		const FString Name = Relic->GetName();
		TestTrue(Name + TEXT(": a relic in the relic slot, insurable, one a slot"), Relic->ItemType == HollowTags::Item_Type_Relic
			&& Relic->EquipSlot == HollowTags::Item_Slot_Relic && Relic->bInsurable && Relic->MaxStack == 1 && Relic->BaseValue == RelicValue);
		const UHollowAbilitySet* Set = Relic->AbilitySet.LoadSynchronous();
		TestTrue(Name + TEXT(": its set grants one lasting effect, no ability"), Set && Set->GrantedAbilities.IsEmpty() && Set->GrantedEffects.Num() == 1
			&& Set->GrantedEffects[0].GameplayEffect && Set->GrantedEffects[0].GameplayEffect.GetDefaultObject()->DurationPolicy == EGameplayEffectDurationType::Infinite);
		const FPrimaryAssetId Id = Relic->GetPrimaryAssetId();
		TestTrue(Name + TEXT(": made on the workbench"), Algo::AnyOf(Terms.Recipes, [&Id](const TPair<FPrimaryAssetId, FHollowRecipeTerms>& Recipe)
		{
			return Recipe.Value.Output.Item == Id;
		}));
		TestFalse(Name + TEXT(": not in the shop"), Terms.ShopPrices.Contains(Id));
	}

	const int32* Relics = Terms.EquipSlotLimits.Find(HollowTags::Item_Slot_Relic);
	TestTrue(TEXT("Limits: one tool a hand, one gadget, one relic"), Terms.EquipSlotLimits.FindRef(HollowTags::Item_Slot_Primary) == 1
		&& Terms.EquipSlotLimits.FindRef(HollowTags::Item_Slot_Secondary) == 1 && Terms.EquipSlotLimits.FindRef(HollowTags::Item_Slot_Gadget) == 1 && Relics && *Relics == 1);
	TestTrue(TEXT("Relics are raid items"), GetDefault<UHollowEconomySettings>()->LoadoutItemTypes.HasTagExact(HollowTags::Item_Type_Relic));
	const TMap<FGameplayTag, int32> WithReliquary = HollowCraftingRules::EquipSlotLimits(Terms.EquipSlotLimits, HollowCraftingRules::SumEffects({Reliquary}, Terms.Upgrades));
	TestEqual(TEXT("The Reliquary: a second relic slot"), WithReliquary.FindRef(HollowTags::Item_Slot_Relic), 2);

	// The raid items' slots, and the classes' starting loadouts within them.
	const TPair<const TCHAR*, FGameplayTag> Slots[] = {
		{TEXT("DA_Item_PlasmaCutter"), HollowTags::Item_Slot_Primary}, {TEXT("DA_Item_AetherLantern"), HollowTags::Item_Slot_Primary},
		{TEXT("DA_Item_Welder"), HollowTags::Item_Slot_Secondary}, {TEXT("DA_Item_PulseMine"), HollowTags::Item_Slot_Gadget},
		{TEXT("DA_Item_OccultWard"), HollowTags::Item_Slot_Gadget}, {TEXT("DA_Item_FuelCell"), FGameplayTag()},
	};
	const FHollowItemCatalog Catalog = FHollowItemCatalog::Load([&Slots]
	{
		TArray<FPrimaryAssetId> Ids;
		for (const TPair<const TCHAR*, FGameplayTag>& Each : Slots)
		{
			Ids.Add(FPrimaryAssetId(UHollowItemDefinition::AssetType, Each.Key));
		}
		return Ids;
	}());
	for (const TPair<const TCHAR*, FGameplayTag>& Each : Slots)
	{
		TestTrue(FString::Printf(TEXT("%s: its equip slot"), Each.Key), Catalog.GetEquipSlot(FPrimaryAssetId(UHollowItemDefinition::AssetType, Each.Key)) == Each.Value);
	}
	for (const TPair<FPrimaryAssetId, FHollowClassTerms>& Class : Terms.Classes)
	{
		TArray<FPrimaryAssetId> Held;
		bool bFits = true;
		for (const FHollowLoadoutSlot& Slot : Class.Value.StartingLoadout)
		{
			bFits &= HollowCraftingRules::CanTakeIntoSlot(Held, Slot.Item, Terms.Catalog, Terms.EquipSlotLimits);
			Held.Add(Slot.Item);
		}
		TestTrue(Class.Key.ToString() + TEXT(": its starting loadout keeps to the slots"), bFits);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
