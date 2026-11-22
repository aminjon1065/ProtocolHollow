#include "Tests/HollowEnemyTestScene.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/HollowAbilitySet.h"
#include "Attributes/HollowResonanceSet.h"
#include "Attributes/HollowToolSet.h"
#include "Audio/HollowSoundBank.h"
#include "Classes/HollowClassDefinition.h"
#include "Crafting/HollowCraftingRules.h"
#include "Data/HollowLanternDefinition.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Items/HollowItemDefinition.h"
#include "Noise/HollowNoiseConfig.h"
#include "Profile/HollowProfileTypes.h"
#include "Tools/HollowGA_AetherLantern.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace HollowAetherLanternTests
{
	using namespace HollowEnemyTests;

	// Content under test (create_lantern_assets.py, create_economy_assets.py, create_class_assets.py, create_noise_assets.py).
	const TCHAR* const LanternAbilityClass = TEXT("/Game/Hollow/Tools/AetherLantern/GA_Tool_AetherLantern.GA_Tool_AetherLantern_C");
	const TCHAR* const LanternItemPath = TEXT("/Game/Hollow/Items/Tools/DA_Item_AetherLantern.DA_Item_AetherLantern");
	const TCHAR* const NoiseConfigPath = TEXT("/Game/Hollow/Core/DA_NoiseConfig.DA_NoiseConfig");
	/** Loaded up front: a cue class still loading when the test ends would spawn into the next test's world. */
	const TCHAR* const LightCueClass = TEXT("/Game/Hollow/GAS/Cues/GCN_Tool_Lantern.GCN_Tool_Lantern_C");
	const TCHAR* const GlowCueClass = TEXT("/Game/Hollow/GAS/Cues/GCN_Status_Exposed.GCN_Status_Exposed_C");
	const FPrimaryAssetId LanternItem(UHollowItemDefinition::AssetType, TEXT("DA_Item_AetherLantern"));
	const FPrimaryAssetId Occultist(UHollowClassDefinition::AssetType, TEXT("DA_Class_Occultist"));

	// The scene (FEnemyScene's open floor, uu): the wielder looks along +X. Pilgrims in the cone, wide of it (50 deg),
	// beyond its reach (12 m) and behind a pillar (20 deg, 7.4 m); a teammate 4 m off, another 13 m off.
	const FVector WielderAt(200.0, -1000.0, 150.0);
	const FVector LitAt(800.0, -1000.0, 150.0);
	const FVector WideAt(700.0, -400.0, 150.0);
	const FVector BeyondAt(1400.0, -1200.0, 150.0);
	const FVector HiddenAt(900.0, -1250.0, 150.0);
	const FVector PillarMin(530.0, -1200.0, 0.0);
	const FVector PillarMax(570.0, -1080.0, 400.0);
	const FVector NearMateAt(200.0, -600.0, 150.0);
	const FVector FarMateAt(200.0, 300.0, 150.0);
	constexpr float SampleSeconds = 0.35f;
	constexpr float FadeSlack = 0.1f;
	constexpr float HeatBelowMax = 0.1f;
	constexpr float SpeedTolerance = 0.01f;

	float Seconds(const TSubclassOf<UGameplayEffect>& Effect)
	{
		float Value = 0.f;
		return Effect && Effect.GetDefaultObject()->DurationMagnitude.GetStaticMagnitudeIfPossible(1.f, Value) ? Value : 0.f;
	}

	bool IsExposed(const AHollowEnemyCharacter* Enemy)
	{
		return Enemy->GetAbilitySystemComponent()->HasMatchingGameplayTag(HollowTags::State_Exposed);
	}

	bool InAura(const UAbilitySystemComponent* AbilitySystem)
	{
		return AbilitySystem->HasMatchingGameplayTag(HollowTags::State_AntiResonance);
	}

	float Armor(const AHollowEnemyCharacter* Enemy)
	{
		return Enemy->GetAbilitySystemComponent()->GetNumericAttribute(UHollowHealthSet::GetArmorAttribute());
	}

	void SetAttribute(UAbilitySystemComponent* AbilitySystem, const FGameplayAttribute& Attribute, const float Value)
	{
		UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
		Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
		FGameplayModifierInfo& Modifier = Effect->Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Override;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Value));
		AbilitySystem->ApplyGameplayEffectToSelf(Effect, 1.f, AbilitySystem->MakeEffectContext());
	}

	/**
	 * A pillar after the scene has begun play: movable, since a static mesh component refuses a new mesh in a running
	 * world (FEnemyScene's boxes come before).
	 */
	void SpawnPillar(UWorld* World, const FVector& Min, const FVector& Max)
	{
		const FTransform Transform(FQuat::Identity, (Min + Max) * 0.5, (Max - Min) / CubeSize);
		AStaticMeshActor* Pillar = World->SpawnActorDeferred<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform);
		Pillar->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		Pillar->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, CubeMesh));
		Pillar->FinishSpawning(Transform);
	}

	FGameplayAbilitySpecHandle Give(UAbilitySystemComponent* AbilitySystem, const TSubclassOf<UGameplayAbility>& Ability, const FGameplayTag& Input)
	{
		FGameplayAbilitySpec Spec(Ability, 1);
		Spec.GetDynamicSpecSourceTags().AddTag(Input);
		return AbilitySystem->GiveAbility(Spec);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowAetherLanternBeamTest, "Hollow.GAS.AetherLantern.Beam",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowAetherLanternBeamTest::RunTest(const FString& Parameters)
{
	// ADR-0035, S6-02 acceptance: held, the lantern burns fuel and heats like every tool; its samples expose the enemies
	// in its cone and in sight - not wide of it, beyond its reach or behind cover - slowing their moves and weakening
	// their armour, and put the aura on the living players near the wielder, the wielder too; released, both fade.
	// One continuous tool runs at a time; overheating puts the lantern out.
	using namespace HollowAetherLanternTests;
	FEnemyScene Scene;
	const TSubclassOf<UGameplayAbility> LanternClass = LoadClass<UGameplayAbility>(nullptr, LanternAbilityClass);
	const TSubclassOf<UGameplayAbility> WelderClass = LoadClass<UGameplayAbility>(nullptr, WelderAbilityClass);
	if (!TestNotNull(TEXT("GA_Tool_AetherLantern"), LanternClass.Get()) || !TestNotNull(TEXT("GA_Tool_Welder"), WelderClass.Get())
		|| !TestNotNull(TEXT("GCN_Tool_Lantern"), LoadClass<AActor>(nullptr, LightCueClass)) || !TestNotNull(TEXT("GCN_Status_Exposed"), LoadClass<AActor>(nullptr, GlowCueClass))
		|| !TestNotNull(TEXT("GCN_Tool_Beam"), LoadClass<AActor>(nullptr, BeamCueClass)) || !Scene.Create(*this, EWall::None))
	{
		return false;
	}
	SpawnPillar(Scene.World, PillarMin, PillarMax);
	AHollowExoFrameCharacter* Wielder = nullptr;
	AHollowExoFrameCharacter* NearMate = nullptr;
	AHollowExoFrameCharacter* FarMate = nullptr;
	UHollowAbilitySystemComponent* Abilities = Scene.SpawnPlayer(*this, WielderAt, Wielder, 0.f, /*bLocalPlayer=*/true);
	UHollowAbilitySystemComponent* NearAbilities = Scene.SpawnPlayer(*this, NearMateAt, NearMate);
	UHollowAbilitySystemComponent* FarAbilities = Scene.SpawnPlayer(*this, FarMateAt, FarMate);
	AHollowEnemyCharacter* Lit = Scene.SpawnPilgrim(*this, LitAt);
	AHollowEnemyCharacter* Wide = Scene.SpawnPilgrim(*this, WideAt);
	AHollowEnemyCharacter* Beyond = Scene.SpawnPilgrim(*this, BeyondAt);
	AHollowEnemyCharacter* Hidden = Scene.SpawnPilgrim(*this, HiddenAt);
	const UHollowLanternDefinition* Lantern = CastChecked<UHollowGA_AetherLantern>(LanternClass.GetDefaultObject())->GetLanternDefinition();
	if (!Abilities || !NearAbilities || !FarAbilities || !Lit || !Wide || !Beyond || !Hidden || !TestNotNull(TEXT("DA_Tool_AetherLantern"), Lantern))
	{
		return false;
	}
	for (const AHollowEnemyCharacter* Enemy : {Lit, Wide, Beyond, Hidden})
	{
		Cast<AHollowAIController>(Enemy->GetController())->Shutdown();   // targets that stay put (their brains would hear the hum)
	}
	Wielder->GetController()->SetControlRotation(FRotator::ZeroRotator);
	const FGameplayAbilitySpecHandle LanternHandle = Give(Abilities, LanternClass, HollowTags::Input_PrimaryUse);
	const FGameplayAbilitySpecHandle WelderHandle = Give(Abilities, WelderClass, HollowTags::Input_SecondaryUse);
	auto IsRunning = [Abilities](const FGameplayAbilitySpecHandle Handle)
	{
		const FGameplayAbilitySpec* Spec = Abilities->FindAbilitySpecFromHandle(Handle);
		return Spec && Spec->IsActive();
	};
	const UCharacterMovementComponent* LitMovement = Lit->GetCharacterMovement();
	const float Walk = LitMovement->MaxWalkSpeed;
	const float LitArmor = Armor(Lit);
	const FGameplayAttribute Gain = UHollowResonanceSet::GetResonanceGainMultiplierAttribute();
	const float FullGain = FarAbilities->GetNumericAttribute(Gain);
	const float Fuel = Abilities->GetNumericAttribute(UHollowToolSet::GetToolFuelAttribute());

	// Held for a few samples.
	Abilities->AbilityInputTagPressed(HollowTags::Input_PrimaryUse);
	TestTrue(TEXT("Pressing the input lights the lantern"), IsRunning(LanternHandle));
	Advance(Scene.TestWorld, SampleSeconds);
	TestTrue(TEXT("The Pilgrim in the cone is exposed"), IsExposed(Lit));
	TestFalse(TEXT("...not the one wide of it"), IsExposed(Wide));
	TestFalse(TEXT("...nor the one beyond its reach"), IsExposed(Beyond));
	TestFalse(TEXT("...nor the one behind the pillar"), IsExposed(Hidden));
	const float Multiplier = Cast<AHollowAIController>(Lit->GetController())->GetMoveSpeedMultiplier();
	TestTrue(FString::Printf(TEXT("...slowed: x%.2f, %.0f of %.0f cm/s"), Multiplier, LitMovement->MaxWalkSpeed, Walk), Multiplier < 1.f && Walk > 0.f
		&& FMath::IsNearlyEqual(LitMovement->MaxWalkSpeed, Walk * Multiplier, Walk * SpeedTolerance));
	TestTrue(TEXT("...its armour weaker"), Armor(Lit) < LitArmor);
	TestTrue(TEXT("The aura: on the wielder and the teammate near"), InAura(Abilities) && InAura(NearAbilities)
		&& NearAbilities->GetNumericAttribute(Gain) < FullGain);
	TestTrue(TEXT("...not on the one far off"), !InAura(FarAbilities) && FMath::IsNearlyEqual(FarAbilities->GetNumericAttribute(Gain), FullGain));
	TestTrue(TEXT("Fuel burns, the lantern heats"), Abilities->GetNumericAttribute(UHollowToolSet::GetToolFuelAttribute()) < Fuel
		&& Abilities->GetNumericAttribute(UHollowToolSet::GetToolHeatAttribute()) > 0.f);

	// One continuous tool at a time.
	Abilities->AbilityInputTagPressed(HollowTags::Input_SecondaryUse);
	TestFalse(TEXT("The welder does not start while the lantern runs"), IsRunning(WelderHandle));
	Abilities->AbilityInputTagReleased(HollowTags::Input_SecondaryUse);

	// Released: the light and the aura fade.
	Abilities->AbilityInputTagReleased(HollowTags::Input_PrimaryUse);
	TestFalse(TEXT("Releasing the input puts it out"), IsRunning(LanternHandle));
	Advance(Scene.TestWorld, FMath::Max(Seconds(Lantern->ExposeEffect), Seconds(Lantern->AuraEffect)) + FadeSlack);
	TestTrue(TEXT("The light fades: the Pilgrim's speed and armour back"), !IsExposed(Lit) && FMath::IsNearlyEqual(LitMovement->MaxWalkSpeed, Walk)
		&& FMath::IsNearlyEqual(Armor(Lit), LitArmor));
	TestTrue(TEXT("...the aura too"), !InAura(Abilities) && !InAura(NearAbilities) && FMath::IsNearlyEqual(NearAbilities->GetNumericAttribute(Gain), FullGain));
	Abilities->AbilityInputTagPressed(HollowTags::Input_SecondaryUse);
	TestTrue(TEXT("...and the welder starts again"), IsRunning(WelderHandle));
	Abilities->AbilityInputTagReleased(HollowTags::Input_SecondaryUse);

	// Overheating puts it out.
	SetAttribute(Abilities, UHollowToolSet::GetToolHeatAttribute(), Abilities->GetNumericAttribute(UHollowToolSet::GetMaxToolHeatAttribute()) - HeatBelowMax);
	Abilities->AbilityInputTagPressed(HollowTags::Input_PrimaryUse);
	Advance(Scene.TestWorld, 2.f / Lantern->SampleRateHz);
	TestTrue(TEXT("Overheated, the lantern goes out"), !IsRunning(LanternHandle) && Abilities->HasMatchingGameplayTag(HollowTags::State_Tool_Overheated));
	Abilities->AbilityInputTagReleased(HollowTags::Input_PrimaryUse);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowAetherLanternDataTest, "Hollow.GAS.AetherLantern.Data",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowAetherLanternDataTest::RunTest(const FString& Parameters)
{
	// The shipped lantern (create_lantern_assets.py): a valid lantern definition behind a tool ability (carrying a core
	// blocks it), an insurable tool item granting it on the primary input, sold in the shop above its value, in the
	// Occultist's starting loadout insured; its hum is a quiet noise the AI hears and players hear.
	using namespace HollowAetherLanternTests;
	const TSubclassOf<UGameplayAbility> LanternClass = LoadClass<UGameplayAbility>(nullptr, LanternAbilityClass);
	const UHollowItemDefinition* Item = LoadObject<UHollowItemDefinition>(nullptr, LanternItemPath);
	const UHollowGA_AetherLantern* Ability = LanternClass ? Cast<UHollowGA_AetherLantern>(LanternClass.GetDefaultObject()) : nullptr;
	const UHollowLanternDefinition* Lantern = Ability ? Ability->GetLanternDefinition() : nullptr;
	if (!TestNotNull(TEXT("GA_Tool_AetherLantern"), Ability) || !TestNotNull(TEXT("DA_Tool_AetherLantern"), Lantern) || !TestNotNull(TEXT("DA_Item_AetherLantern"), Item))
	{
		return false;
	}
#if WITH_EDITOR
	FDataValidationContext Validation;
	TestTrue(TEXT("The lantern definition is valid"), Lantern->IsDataValid(Validation) != EDataValidationResult::Invalid && Validation.GetNumErrors() == 0);
#endif
	TestTrue(TEXT("A tool: its tag, which carrying blocks"), Lantern->ToolTag == HollowTags::Ability_Tool_AetherLantern
		&& Ability->GetAssetTags().HasTag(HollowTags::Ability_Tool_AetherLantern) && Ability->GetAssetTags().HasTag(HollowTags::Ability_Tool));
	TestTrue(TEXT("The item: an insurable tool, one per slot"), Item->ItemType == HollowTags::Item_Type_Tool && Item->bInsurable && Item->MaxStack == 1);
	const UHollowAbilitySet* Set = Item->AbilitySet.LoadSynchronous();
	TestTrue(TEXT("...granting the lantern on the primary input"), Set && Set->GrantedAbilities.Num() == 1 && Set->GrantedAbilities[0].Ability == LanternClass
		&& Set->GrantedAbilities[0].InputTag == HollowTags::Input_PrimaryUse);

	const FHollowEconomyTerms Terms = FHollowEconomyTerms::Load(FHollowProfileState());
	const int32* Price = Terms.ShopPrices.Find(LanternItem);
	TestTrue(TEXT("Sold in the shop, above what selling it pays"), Price && *Price > Item->BaseValue * Terms.SellPercent / 100);
	const FHollowClassTerms* Class = Terms.Classes.Find(Occultist);
	TestTrue(TEXT("In the Occultist's starting loadout, insured"), Class && Class->StartingLoadout.ContainsByPredicate([](const FHollowLoadoutSlot& Slot)
	{
		return Slot.Item == LanternItem && Slot.bInsured;
	}));

	const UHollowNoiseConfig* Noise = LoadObject<UHollowNoiseConfig>(nullptr, NoiseConfigPath);
	TestTrue(TEXT("Its hum: a noise rule"), Noise && Lantern->NoiseTag.IsValid() && Noise->FindRule(Lantern->NoiseTag));
	const UHollowSoundBank* Bank = UHollowSoundBank::Get();
	TestTrue(TEXT("...and a sound"), Bank && Bank->Entries.ContainsByPredicate([Lantern](const FHollowSoundEntry& Entry) { return Entry.SoundTag == Lantern->NoiseTag; }));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
