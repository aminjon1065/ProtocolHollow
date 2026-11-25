#include "Tests/HollowEnemyTestScene.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/HollowAbilitySet.h"
#include "Audio/HollowSoundBank.h"
#include "Attributes/HollowWeaponSet.h"
#include "BrainComponent.h"
#include "Components/CapsuleComponent.h"
#include "Data/HollowWeaponDefinition.h"
#include "Enemies/HollowEnemyDefinition.h"
#include "Feel/HollowFeelConfig.h"
#include "Feel/HollowFeelSubsystem.h"
#include "Feel/HollowViewFeelComponent.h"
#include "Feel/HollowViewFeelModifier.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/HollowInventoryComponent.h"
#include "Items/HollowItemDefinition.h"
#include "Noise/HollowNoiseSubsystem.h"
#include "Noise/HollowNoiseTypes.h"
#include "Settings/HollowGameUserSettings.h"
#include "Tools/HollowToolMountComponent.h"
#include "Weapons/HollowGA_MeleeBash.h"
#include "Weapons/HollowGA_WeaponFire.h"
#include "Weapons/HollowHandsComponent.h"
#include "Weapons/HollowWeaponShotData.h"

namespace HollowWeaponGameTests
{
	// Content under test (create_weapon_assets.py, create_class_assets.py, create_lantern_assets.py, create_carry_assets.py).
	const TCHAR* const NailgunItem = TEXT("/Game/Hollow/Items/Weapons/DA_Item_Weapon_Nailgun.DA_Item_Weapon_Nailgun");
	const TCHAR* const ShotgunItem = TEXT("/Game/Hollow/Items/Weapons/DA_Item_Weapon_Shotgun.DA_Item_Weapon_Shotgun");
	const TCHAR* const NailsItem = TEXT("/Game/Hollow/Items/Ammo/DA_Item_Ammo_Nails.DA_Item_Ammo_Nails");
	const TCHAR* const ShellsItem = TEXT("/Game/Hollow/Items/Ammo/DA_Item_Ammo_Shells.DA_Item_Ammo_Shells");
	const TCHAR* const SuitSet = TEXT("/Game/Hollow/Characters/ExoFrame/DA_AbilitySet_ExoFrame.DA_AbilitySet_ExoFrame");
	const TCHAR* const MeleeAbility = TEXT("/Game/Hollow/Characters/ExoFrame/GA_MeleeBash.GA_MeleeBash_C");
	/** Loaded up front: a cue class still loading when the test ends would spawn into the next test's world. */
	const TCHAR* const FireCue = TEXT("/Game/Hollow/GAS/Cues/GCN_Weapon_Fire.GCN_Weapon_Fire_C");
	const TCHAR* const ExposeEffect = TEXT("/Game/Hollow/GAS/Effects/GE_Occult_Expose.GE_Occult_Expose_C");
	const TCHAR* const CarryEffect = TEXT("/Game/Hollow/GAS/Effects/GE_Carry_Solo.GE_Carry_Solo_C");

	/** Test data: the shooter in room A of the open scene, an enemy a few metres ahead. */
	const FVector ShooterAt(400.0, 0.0, 100.0);
	const FVector EnemyAt(800.0, 0.0, 100.0);
	const FVector CloseEnemyAt(600.0, 0.0, 100.0);
	constexpr float SwapWait = 0.3f;
	constexpr int32 NailsCarried = 40;
	constexpr int32 ShellsCarried = 8;

	struct FRig
	{
		HollowEnemyTests::FEnemyScene Scene;
		AHollowExoFrameCharacter* Character = nullptr;
		UHollowAbilitySystemComponent* AbilitySystem = nullptr;
		UHollowHandsComponent* Hands = nullptr;
		UHollowInventoryComponent* Inventory = nullptr;
		FHollowAbilitySetHandles Suit;
	};

	void Advance(FRig& Rig, const float Seconds)
	{
		HollowEnemyTests::Advance(Rig.Scene.TestWorld, Seconds);
	}

	/** A local player (the listen-server host: its own shots are applied as they are) with the suit's abilities. */
	bool Build(FAutomationTestBase& Test, FRig& Rig)
	{
		if (!Rig.Scene.Create(Test, HollowEnemyTests::EWall::None) || !Test.TestNotNull(TEXT("GCN_Weapon_Fire"), LoadClass<UObject>(nullptr, FireCue)))
		{
			return false;
		}
		Rig.AbilitySystem = Rig.Scene.SpawnPlayer(Test, ShooterAt, Rig.Character, 0.f, /*bLocalPlayer=*/true);
		const UHollowAbilitySet* Suit = LoadObject<UHollowAbilitySet>(nullptr, SuitSet);
		if (!Test.TestNotNull(TEXT("Player"), Rig.AbilitySystem) || !Test.TestNotNull(TEXT("DA_AbilitySet_ExoFrame"), Suit))
		{
			return false;
		}
		Suit->GiveToAbilitySystem(Rig.AbilitySystem, &Rig.Suit, Rig.Character);
		Rig.Hands = Rig.Character->GetHands();
		Rig.Inventory = UHollowInventoryComponent::Get(Rig.Character->GetPlayerState());
		return Test.TestNotNull(TEXT("Hands"), Rig.Hands) && Test.TestNotNull(TEXT("Raid inventory"), Rig.Inventory);
	}

	UHollowItemDefinition* Item(const TCHAR* Path)
	{
		return LoadObject<UHollowItemDefinition>(nullptr, Path);
	}

	void Press(const FRig& Rig, const FGameplayTag& Input)
	{
		Rig.AbilitySystem->AbilityInputTagPressed(Input);
		Rig.AbilitySystem->AbilityInputTagReleased(Input);
	}

	/** The weapon in hand, its swap waited out. */
	void DrawWeapon(FRig& Rig)
	{
		Press(Rig, HollowTags::Input_Hands_Weapon);
		Advance(Rig, SwapWait);
	}

	void AimAt(const FRig& Rig, const AActor& Target)
	{
		FVector Origin;
		FVector Aim;
		UHollowHandsComponent::GetAimView(*Rig.Character, Origin, Aim);
		Rig.Character->GetController()->SetControlRotation((Target.GetActorLocation() - Origin).Rotation());
	}

	float Magazine(const FRig& Rig)
	{
		return Rig.AbilitySystem->GetNumericAttribute(UHollowWeaponSet::GetMagazineAttribute());
	}

	void SetMagazine(const FRig& Rig, const float Value)
	{
		UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
		Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
		FGameplayModifierInfo& Modifier = Effect->Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = UHollowWeaponSet::GetMagazineAttribute();
		Modifier.ModifierOp = EGameplayModOp::Override;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Value));
		Rig.AbilitySystem->ApplyGameplayEffectToSelf(Effect, 1.f, Rig.AbilitySystem->MakeEffectContext());
	}

	/** An enemy that stays where it is (its brain stopped): only the shots move it. */
	AHollowEnemyCharacter* SpawnStillPilgrim(FAutomationTestBase& Test, FRig& Rig, const FVector& Location)
	{
		AHollowEnemyCharacter* Pilgrim = Rig.Scene.SpawnPilgrim(Test, Location);
		AAIController* Brain = Pilgrim ? Cast<AAIController>(Pilgrim->GetController()) : nullptr;
		if (Brain && Brain->GetBrainComponent())
		{
			Brain->GetBrainComponent()->StopLogic(TEXT("Hollow.Game.Weapons test"));
			Brain->StopMovement();   // its first patrol step too
		}
		Advance(Rig, 0.5f);   // it settles on the floor
		return Pilgrim;
	}

	float PelletDamageOn(const UAbilitySystemComponent& Target, const UHollowWeaponDefinition& Weapon, const bool bExposed)
	{
		const float Armor = Target.GetNumericAttribute(UHollowHealthSet::GetArmorAttribute());
		const float Penetration = Weapon.ArmorPenetration + (bExposed ? Weapon.ExposedPenetrationBonus : 0.f);
		return HollowDamageMath::Mitigate(Weapon.DamagePerPellet, Armor, GetDefault<UHollowDamageSettings>()->ArmorConstant, true, false, Penetration);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowWeaponDataTest, "Hollow.Game.Weapons.Data",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowWeaponDataTest::RunTest(const FString& Parameters)
{
	// GP-01 (ADR-0044): two weapons in the weapon slot, their abilities on the shooter's inputs, the suit's swap and blow;
	// the shotgun is the Pilgrim's answer (16 blasts, about 11 exposed), the nailgun is not.
	using namespace HollowWeaponGameTests;
	const UHollowItemDefinition* Items[] = {Item(NailgunItem), Item(ShotgunItem)};
	for (const UHollowItemDefinition* Weapon : Items)
	{
		if (!TestNotNull(TEXT("Weapon item"), Weapon))
		{
			return false;
		}
		TestTrue(Weapon->GetName() + TEXT(" is a weapon in the weapon slot"), Weapon->ItemType == HollowTags::Item_Type_Weapon
			&& Weapon->EquipSlot == HollowTags::Item_Slot_Weapon && Weapon->MaxStack == 1 && Weapon->bInsurable);
		const UHollowAbilitySet* Set = Weapon->AbilitySet.LoadSynchronous();
		const FHollowAbilitySetAbility* Fire = Set ? Set->GrantedAbilities.FindByPredicate([](const FHollowAbilitySetAbility& Entry)
		{
			return Entry.Ability && Entry.Ability->IsChildOf<UHollowGA_WeaponFire>();
		}) : nullptr;
		if (!TestNotNull(Weapon->GetName() + TEXT(" grants its fire ability"), Fire))
		{
			continue;
		}
		TestTrue(TEXT("...on the primary input"), Fire->InputTag == HollowTags::Input_PrimaryUse);
		const UHollowWeaponDefinition* Definition = Fire->Ability->GetDefaultObject<UHollowGA_WeaponFire>()->GetWeaponDefinition();
		if (TestNotNull(TEXT("...firing its weapon"), Definition))
		{
			TestTrue(Definition->GetName() + TEXT(": its rounds are ammunition"), Item(*FString::Printf(TEXT("/Game/Hollow/Items/Ammo/%s.%s"),
				*Definition->AmmoItem.PrimaryAssetName.ToString(), *Definition->AmmoItem.PrimaryAssetName.ToString())) != nullptr);
			TestTrue(Definition->GetName() + TEXT(": a localizable name"), Definition->DisplayName.IsFromStringTable());
			TestTrue(Definition->GetName() + TEXT(": its damage effect and noise"), Definition->DamageEffect && Definition->NoiseTag.IsValid());
		}
		TestEqual(Weapon->GetName() + TEXT(": fire, dry fire, aim, reload"), Set->GrantedAbilities.Num(), 4);
		TestEqual(Weapon->GetName() + TEXT(": its magazine with it"), Set->GrantedEffects.Num(), 1);
	}
	for (const TCHAR* Path : {NailsItem, ShellsItem})
	{
		const UHollowItemDefinition* Ammo = Item(Path);
		TestTrue(FString(Path) + TEXT(": ammunition without a slot"), Ammo && Ammo->ItemType == HollowTags::Item_Type_Ammo && !Ammo->EquipSlot.IsValid()
			&& Ammo->MaxStack > 1 && Ammo->DisplayName.IsFromStringTable());
	}

	const UHollowAbilitySet* Suit = LoadObject<UHollowAbilitySet>(nullptr, SuitSet);
	int32 Swaps = 0;
	int32 Blows = 0;
	for (const FHollowAbilitySetAbility& Entry : Suit ? Suit->GrantedAbilities : TArray<FHollowAbilitySetAbility>())
	{
		Swaps += Entry.Ability && Entry.Ability->GetName().Contains(TEXT("GA_SwapHands")) ? 1 : 0;
		Blows += Entry.Ability && Entry.Ability->IsChildOf<UHollowGA_MeleeBash>() && Entry.InputTag == HollowTags::Input_Melee ? 1 : 0;
	}
	TestEqual(TEXT("The suit swaps on the tools, weapon and cycle inputs"), Swaps, 3);
	TestEqual(TEXT("The suit strikes on the melee input"), Blows, 1);

	// The Pilgrim's numbers: its armour, the shotgun's full blast.
	const UHollowItemDefinition* Shotgun = Item(ShotgunItem);
	const UHollowAbilitySet* ShotgunSet = Shotgun ? Shotgun->AbilitySet.LoadSynchronous() : nullptr;
	const UHollowWeaponDefinition* Blast = nullptr;
	for (const FHollowAbilitySetAbility& Entry : ShotgunSet ? ShotgunSet->GrantedAbilities : TArray<FHollowAbilitySetAbility>())
	{
		if (Entry.Ability && Entry.Ability->IsChildOf<UHollowGA_WeaponFire>())
		{
			Blast = Entry.Ability->GetDefaultObject<UHollowGA_WeaponFire>()->GetWeaponDefinition();
		}
	}
	if (TestNotNull(TEXT("The shotgun"), Blast))
	{
		const float K = GetDefault<UHollowDamageSettings>()->ArmorConstant;
		const float Shot = Blast->Pellets * HollowDamageMath::Mitigate(Blast->DamagePerPellet, 100.f, K, true, false, Blast->ArmorPenetration);
		const float Exposed = Blast->Pellets * HollowDamageMath::Mitigate(Blast->DamagePerPellet, 50.f, K, true, false,
			Blast->ArmorPenetration + Blast->ExposedPenetrationBonus);
		const int32 Blasts = FMath::CeilToInt32(2000.f / Shot);
		const int32 ExposedBlasts = FMath::CeilToInt32(2000.f / Exposed);
		TestTrue(FString::Printf(TEXT("The Pilgrim takes 14..18 blasts (%d), 9..13 exposed (%d)"), Blasts, ExposedBlasts),
			Blasts >= 14 && Blasts <= 18 && ExposedBlasts >= 9 && ExposedBlasts <= 13);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowWeaponHandsTest, "Hollow.Game.Weapons.Hands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowWeaponHandsTest::RunTest(const FString& Parameters)
{
	// GP-01: the weapon comes from the raid inventory; 1 / 2 / the cycle switch the hands for a moment; the tools are
	// blocked with the weapon in hand and the weapon with the tools; carrying a core keeps the hands as they are.
	using namespace HollowWeaponGameTests;
	FRig Rig;
	if (!Build(*this, Rig))
	{
		return false;
	}
	const TSubclassOf<UGameplayAbility> WelderClass = LoadClass<UGameplayAbility>(nullptr, HollowEnemyTests::WelderAbilityClass);
	FGameplayAbilitySpec WelderSpec(WelderClass, 1);
	WelderSpec.GetDynamicSpecSourceTags().AddTag(HollowTags::Input_SecondaryUse);
	const FGameplayAbilitySpecHandle Welder = Rig.AbilitySystem->GiveAbility(WelderSpec);
	auto Welding = [&Rig, Welder]
	{
		const FGameplayAbilitySpec* Spec = Rig.AbilitySystem->FindAbilitySpecFromHandle(Welder);
		return Spec && Spec->IsActive();
	};
	UHollowToolMountComponent* Mount = Rig.Character->GetToolMount();

	Press(Rig, HollowTags::Input_Hands_Weapon);
	TestEqual(TEXT("No weapon carried: the tools stay in hand"), Rig.Hands->GetMode(), EHollowHandsMode::Tools);

	Rig.Inventory->AddItem(Item(NailgunItem), 1);
	Rig.Inventory->AddItem(Item(NailsItem), NailsCarried);
	TestNotNull(TEXT("The nailgun of the raid inventory reaches the hands"), Rig.Hands->GetWeapon());
	Rig.AbilitySystem->AbilityInputTagPressed(HollowTags::Input_SecondaryUse);
	TestTrue(TEXT("Tools in hand: the welder works"), Welding());
	Rig.AbilitySystem->AbilityInputTagReleased(HollowTags::Input_SecondaryUse);

	Press(Rig, HollowTags::Input_Hands_Weapon);
	TestTrue(TEXT("2: the weapon in hand"), Rig.Hands->IsWeaponInHand());
	TestTrue(TEXT("...its tag on the ability system"), Rig.AbilitySystem->HasMatchingGameplayTag(HollowTags::State_Hands_Weapon));
	TestTrue(TEXT("...the hands busy for a moment"), Rig.AbilitySystem->HasMatchingGameplayTag(HollowTags::State_Hands_Swapping));
	TestTrue(TEXT("...the weapon shown in hand"), Mount->GetHeldWeapon() == Rig.Hands->GetWeapon());
	Press(Rig, HollowTags::Input_Hands_Tools);
	TestTrue(TEXT("No second switch while the hands are busy"), Rig.Hands->IsWeaponInHand());
	Advance(Rig, SwapWait);
	TestFalse(TEXT("The moment passes"), Rig.AbilitySystem->HasMatchingGameplayTag(HollowTags::State_Hands_Swapping));
	Rig.AbilitySystem->AbilityInputTagPressed(HollowTags::Input_SecondaryUse);
	TestFalse(TEXT("Weapon in hand: the welder is blocked"), Welding());
	TestTrue(TEXT("...the secondary input aims instead"), Rig.AbilitySystem->HasMatchingGameplayTag(HollowTags::State_Weapon_Aiming));
	Rig.AbilitySystem->AbilityInputTagReleased(HollowTags::Input_SecondaryUse);
	TestFalse(TEXT("Released: no more aiming"), Rig.AbilitySystem->HasMatchingGameplayTag(HollowTags::State_Weapon_Aiming));

	Press(Rig, HollowTags::Input_Hands_Cycle);
	TestEqual(TEXT("The cycle: back to the tools"), Rig.Hands->GetMode(), EHollowHandsMode::Tools);
	TestNull(TEXT("...the tool shown again"), Mount->GetHeldWeapon());
	TestFalse(TEXT("...the weapon's tag gone"), Rig.AbilitySystem->HasMatchingGameplayTag(HollowTags::State_Hands_Weapon));
	Advance(Rig, SwapWait);
	Rig.AbilitySystem->AbilityInputTagPressed(HollowTags::Input_PrimaryUse);
	TestEqual(TEXT("Tools in hand: the trigger fires nothing"), Magazine(Rig), static_cast<float>(Rig.Hands->GetWeapon()->MagazineSize));
	Rig.AbilitySystem->AbilityInputTagReleased(HollowTags::Input_PrimaryUse);

	const TSubclassOf<UGameplayEffect> Carry = LoadClass<UGameplayEffect>(nullptr, CarryEffect);
	if (TestNotNull(TEXT("GE_Carry_Solo"), Carry.Get()))
	{
		const FActiveGameplayEffectHandle Carrying = Rig.AbilitySystem->ApplyGameplayEffectToSelf(Carry.GetDefaultObject(), 1.f, Rig.AbilitySystem->MakeEffectContext());
		Press(Rig, HollowTags::Input_Hands_Weapon);
		TestEqual(TEXT("Carrying a core: no switch"), Rig.Hands->GetMode(), EHollowHandsMode::Tools);
		Rig.AbilitySystem->RemoveActiveGameplayEffect(Carrying);
	}
	Press(Rig, HollowTags::Input_Hands_Weapon);
	TestTrue(TEXT("Put down: the weapon again"), Rig.Hands->IsWeaponInHand());
	Rig.Inventory->Clear();
	TestEqual(TEXT("The weapon gone from the inventory: the tools in hand"), Rig.Hands->GetMode(), EHollowHandsMode::Tools);
	TestFalse(TEXT("...and its tag"), Rig.AbilitySystem->HasMatchingGameplayTag(HollowTags::State_Hands_Weapon));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowWeaponFireTest, "Hollow.Game.Weapons.Fire",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowWeaponFireTest::RunTest(const FString& Parameters)
{
	// GP-01: the nailgun on a Pilgrim: a round a shot at the weapon's pace, damage through the armour (more exposed),
	// a noise for the AI; empty, the trigger clicks and the reload loads what the inventory carries.
	using namespace HollowWeaponGameTests;
	FRig Rig;
	if (!Build(*this, Rig))
	{
		return false;
	}
	Rig.Inventory->AddItem(Item(NailgunItem), 1);
	Rig.Inventory->AddItem(Item(NailsItem), NailsCarried);
	const UHollowWeaponDefinition* Weapon = Rig.Hands->GetWeapon();
	AHollowEnemyCharacter* Pilgrim = SpawnStillPilgrim(*this, Rig, EnemyAt);
	if (!TestNotNull(TEXT("Nailgun"), Weapon) || !Pilgrim)
	{
		return false;
	}
	UAbilitySystemComponent* Target = Pilgrim->GetAbilitySystemComponent();
	DrawWeapon(Rig);
	int32 ShotNoises = 0;
	UHollowNoiseSubsystem::Get(Rig.Scene.World)->OnNoiseReported.AddLambda([&ShotNoises, Weapon](const FHollowNoiseReport& Report)
	{
		ShotNoises += Report.Event.NoiseTag == Weapon->NoiseTag ? 1 : 0;
	});
	const float Size = static_cast<float>(Weapon->MagazineSize);
	TestEqual(TEXT("A full magazine with the weapon"), Magazine(Rig), Size);

	AimAt(Rig, *Pilgrim);
	float Before = HollowEnemyTests::Health(Target);
	Press(Rig, HollowTags::Input_PrimaryUse);
	TestEqual(TEXT("A shot takes a round"), Magazine(Rig), Size - 1.f);
	TestTrue(FString::Printf(TEXT("...and hurts the Pilgrim through its armour (%.2f)"), Before - HollowEnemyTests::Health(Target)),
		FMath::IsNearlyEqual(Before - HollowEnemyTests::Health(Target), PelletDamageOn(*Target, *Weapon, false), 0.05f));
	TestEqual(TEXT("...heard by the AI"), ShotNoises, 1);
	Press(Rig, HollowTags::Input_PrimaryUse);
	TestEqual(TEXT("Faster than the weapon fires: nothing"), Magazine(Rig), Size - 1.f);
	Advance(Rig, Weapon->FireIntervalSeconds + 0.05f);

	const TSubclassOf<UGameplayEffect> Expose = LoadClass<UGameplayEffect>(nullptr, ExposeEffect);
	if (TestNotNull(TEXT("GE_Occult_Expose"), Expose.Get()))
	{
		Target->ApplyGameplayEffectToSelf(Expose.GetDefaultObject(), 1.f, Target->MakeEffectContext());
		const float Expected = PelletDamageOn(*Target, *Weapon, true);
		AimAt(Rig, *Pilgrim);
		Before = HollowEnemyTests::Health(Target);
		Press(Rig, HollowTags::Input_PrimaryUse);
		TestTrue(FString::Printf(TEXT("Exposed: more of it through (%.2f, expected %.2f)"), Before - HollowEnemyTests::Health(Target), Expected),
			FMath::IsNearlyEqual(Before - HollowEnemyTests::Health(Target), Expected, 0.05f) && Expected > PelletDamageOn(*Target, *Weapon, false) - 0.01f);
		Advance(Rig, Weapon->FireIntervalSeconds + 0.05f);
	}

	// Empty: a click, then the reload (its rounds in at once, the weapon busy for its time).
	SetMagazine(Rig, 0.f);
	Before = HollowEnemyTests::Health(Target);
	Press(Rig, HollowTags::Input_PrimaryUse);
	TestEqual(TEXT("Empty: no shot"), HollowEnemyTests::Health(Target), Before);
	TestTrue(TEXT("...the reload starts by itself"), Rig.AbilitySystem->HasMatchingGameplayTag(HollowTags::State_Weapon_Reloading));
	TestEqual(TEXT("...a full magazine loaded"), Magazine(Rig), Size);
	TestEqual(TEXT("...from the raid inventory"), Rig.Inventory->GetCount(Weapon->AmmoItem), NailsCarried - Weapon->MagazineSize);
	Press(Rig, HollowTags::Input_PrimaryUse);
	TestEqual(TEXT("No shot while reloading"), Magazine(Rig), Size);
	Press(Rig, HollowTags::Input_Hands_Tools);
	TestTrue(TEXT("...nor a switch"), Rig.Hands->IsWeaponInHand());
	Advance(Rig, Weapon->ReloadSeconds + 0.1f);
	TestFalse(TEXT("The reload ends"), Rig.AbilitySystem->HasMatchingGameplayTag(HollowTags::State_Weapon_Reloading));
	Press(Rig, HollowTags::Input_Reload);
	TestFalse(TEXT("A full magazine takes no reload"), Rig.AbilitySystem->HasMatchingGameplayTag(HollowTags::State_Weapon_Reloading));
	SetMagazine(Rig, 2.f);
	Rig.Inventory->ConsumeItem(Item(NailsItem), Rig.Inventory->GetCount(Weapon->AmmoItem) - 3);
	Press(Rig, HollowTags::Input_Reload);
	TestEqual(TEXT("R: the last three rounds carried"), Magazine(Rig), 5.f);
	TestEqual(TEXT("...none left"), Rig.Inventory->GetCount(Weapon->AmmoItem), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowWeaponShotgunTest, "Hollow.Game.Weapons.Shotgun",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowWeaponShotgunTest::RunTest(const FString& Parameters)
{
	// GP-01: a blast close up: its pellets' damage in one hit, a push that staggers even the Pilgrim; a squad mate in the
	// way stops the pellets, unharmed (no friendly fire).
	using namespace HollowWeaponGameTests;
	FRig Rig;
	if (!Build(*this, Rig))
	{
		return false;
	}
	Rig.Inventory->AddItem(Item(ShotgunItem), 1);
	Rig.Inventory->AddItem(Item(ShellsItem), ShellsCarried);
	const UHollowWeaponDefinition* Weapon = Rig.Hands->GetWeapon();
	AHollowEnemyCharacter* Pilgrim = SpawnStillPilgrim(*this, Rig, CloseEnemyAt);
	if (!TestNotNull(TEXT("Shotgun"), Weapon) || !Pilgrim)
	{
		return false;
	}
	UAbilitySystemComponent* Target = Pilgrim->GetAbilitySystemComponent();
	DrawWeapon(Rig);
	AimAt(Rig, *Pilgrim);
	const float Before = HollowEnemyTests::Health(Target);
	const int32 HitsBefore = Rig.Hands->GetAcceptedHits();
	const double XBefore = Pilgrim->GetActorLocation().X;
	Press(Rig, HollowTags::Input_PrimaryUse);
	const int32 Pellets = Rig.Hands->GetAcceptedHits() - HitsBefore;
	TestTrue(FString::Printf(TEXT("Most pellets hit up close (%d of %d)"), Pellets, Weapon->Pellets), Pellets >= Weapon->Pellets - 1);
	TestTrue(TEXT("...their damage together"), FMath::IsNearlyEqual(Before - HollowEnemyTests::Health(Target), Pellets * PelletDamageOn(*Target, *Weapon, false), 0.2f));
	TestTrue(TEXT("...the Pilgrim staggered"), Target->HasMatchingGameplayTag(HollowTags::State_Stunned));
	Advance(Rig, 0.5f);
	TestTrue(FString::Printf(TEXT("...and pushed back (%.1f cm)"), Pilgrim->GetActorLocation().X - XBefore), Pilgrim->GetActorLocation().X - XBefore > 5.0);

	AHollowExoFrameCharacter* Mate = nullptr;
	UHollowAbilitySystemComponent* MateAbilities = Rig.Scene.SpawnPlayer(*this, (ShooterAt + Pilgrim->GetActorLocation()) * 0.5, Mate);
	Advance(Rig, Weapon->FireIntervalSeconds + 0.1f);
	if (TestNotNull(TEXT("A squad mate in the way"), MateAbilities))
	{
		AimAt(Rig, *Mate);
		const float MateBefore = HollowEnemyTests::Health(MateAbilities);
		const float PilgrimBefore = HollowEnemyTests::Health(Target);
		Press(Rig, HollowTags::Input_PrimaryUse);
		TestEqual(TEXT("No friendly fire"), HollowEnemyTests::Health(MateAbilities), MateBefore);
		TestEqual(TEXT("...the pellets stopped at the mate"), HollowEnemyTests::Health(Target), PilgrimBefore);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowWeaponServerCheckTest, "Hollow.Game.Weapons.ServerCheck",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowWeaponServerCheckTest::RunTest(const FString& Parameters)
{
	// GP-01 (ADR-0044): a remote owner's hits as the server receives them: one where its view allows is taken, one out
	// of the cone, on another spot or from elsewhere is refused (and counted).
	using namespace HollowWeaponGameTests;
	FRig Rig;
	if (!Build(*this, Rig))
	{
		return false;
	}
	Rig.Inventory->AddItem(Item(NailgunItem), 1);
	AHollowEnemyCharacter* Pilgrim = SpawnStillPilgrim(*this, Rig, EnemyAt);
	const UHollowWeaponDefinition* Weapon = Rig.Hands->GetWeapon();
	UHollowGA_WeaponFire* Fire = nullptr;
	for (const FGameplayAbilitySpec& Spec : Rig.AbilitySystem->GetActivatableAbilities())
	{
		Fire = Fire ? Fire : Cast<UHollowGA_WeaponFire>(Spec.GetPrimaryInstance());
	}
	if (!TestNotNull(TEXT("Nailgun"), Weapon) || !Pilgrim || !TestNotNull(TEXT("Its fire ability, instanced"), Fire))
	{
		return false;
	}
	UAbilitySystemComponent* Target = Pilgrim->GetAbilitySystemComponent();
	AimAt(Rig, *Pilgrim);
	FVector Origin;
	FVector Aim;
	UHollowHandsComponent::GetAimView(*Rig.Character, Origin, Aim);
	TArray<FHitResult> Pellets;
	UHollowGA_WeaponFire::TracePellets(*Rig.Scene.World, *Weapon, Rig.Character, Origin, Aim, 7, false, Pellets);
	FHollowWeaponShotData Shot;
	UHollowGA_WeaponFire::MakeShotData(Pellets, Origin, Aim, 7, false, Shot);
	if (!TestEqual(TEXT("The owner's pellet hits the Pilgrim"), Shot.Hits.Num(), 1) || !TestTrue(TEXT("...the Pilgrim"), Shot.Hits[0].Actor == Pilgrim))
	{
		return false;
	}

	float Before = HollowEnemyTests::Health(Target);
	TestEqual(TEXT("A hit the server's view allows is taken"), Fire->ApplyShot(Shot, /*bTrusted=*/false), 1);
	TestTrue(TEXT("...and hurts"), HollowEnemyTests::Health(Target) < Before);

	auto Refused = [&](const FHollowWeaponShotData& Claim, const TCHAR* What)
	{
		const int32 RejectedBefore = Rig.Hands->GetRejectedHits();
		const float HealthBefore = HollowEnemyTests::Health(Target);
		TestEqual(FString(What) + TEXT(": refused"), Fire->ApplyShot(Claim, /*bTrusted=*/false), 0);
		TestEqual(FString(What) + TEXT(": no damage"), HollowEnemyTests::Health(Target), HealthBefore);
		TestEqual(FString(What) + TEXT(": counted"), Rig.Hands->GetRejectedHits(), RejectedBefore + 1);
	};
	FHollowWeaponShotData Claim = Shot;
	Claim.Aim = FRotator(0.0, 40.0, 0.0).RotateVector(FVector(Aim));
	Refused(Claim, TEXT("A pellet far outside where the server sees it aim"));
	Claim = Shot;
	Claim.Hits[0].Impact = FVector(Pilgrim->GetActorLocation() + FVector(0.0, 600.0, 0.0));
	Refused(Claim, TEXT("An impact nowhere near the Pilgrim"));
	Claim = Shot;
	Claim.Origin = FVector(Origin + FVector(0.0, 0.0, 500.0));
	Refused(Claim, TEXT("Eyes five metres above the server's"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowMeleeTest, "Hollow.Game.Weapons.Melee",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowMeleeTest::RunTest(const FString& Parameters)
{
	// GP-01: the suit's blow, in either hand mode: what it strikes takes its damage through the armour, once a cooldown.
	using namespace HollowWeaponGameTests;
	FRig Rig;
	if (!Build(*this, Rig))
	{
		return false;
	}
	const UHollowGA_MeleeBash* Blow = GetDefault<UHollowGA_MeleeBash>(LoadClass<UHollowGA_MeleeBash>(nullptr, MeleeAbility));
	AHollowEnemyCharacter* Pilgrim = SpawnStillPilgrim(*this, Rig, ShooterAt + FVector(110.0, 0.0, 0.0));
	if (!TestNotNull(TEXT("GA_MeleeBash"), Blow) || !Pilgrim)
	{
		return false;
	}
	UAbilitySystemComponent* Target = Pilgrim->GetAbilitySystemComponent();
	AimAt(Rig, *Pilgrim);
	float Before = HollowEnemyTests::Health(Target);
	Press(Rig, HollowTags::Input_Melee);
	const float Dealt = Before - HollowEnemyTests::Health(Target);
	TestTrue(FString::Printf(TEXT("The blow lands (%.2f)"), Dealt), Dealt > 0.f);
	Before = HollowEnemyTests::Health(Target);
	Press(Rig, HollowTags::Input_Melee);
	TestEqual(TEXT("Once a cooldown"), HollowEnemyTests::Health(Target), Before);
	Advance(Rig, 0.8f);
	Rig.Inventory->AddItem(Item(NailgunItem), 1);
	DrawWeapon(Rig);
	Pilgrim->SetActorLocation(ShooterAt + FVector(110.0, 0.0, Pilgrim->GetActorLocation().Z - ShooterAt.Z));   // back within reach after the push
	AimAt(Rig, *Pilgrim);
	Before = HollowEnemyTests::Health(Target);
	Press(Rig, HollowTags::Input_Melee);
	TestTrue(TEXT("...and with the weapon in hand"), HollowEnemyTests::Health(Target) < Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowFeelDataTest, "Hollow.Game.Feel.Data",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowFeelDataTest::RunTest(const FString& Parameters)
{
	// GP-01c (ADR-0046): every feel the game emits has its entry in DA_FeelConfig, every sound one is felt by plays,
	// and the world's heavy moments shake: a breach, a blast, a Pilgrim's step, a crate breaking.
	using namespace HollowWeaponGameTests;
	const UHollowFeelConfig* Config = UHollowFeelConfig::Get();
	const UHollowSoundBank* Bank = UHollowSoundBank::Get();
	if (!TestNotNull(TEXT("DA_FeelConfig (UHollowFeelSettings::Config)"), Config) || !TestNotNull(TEXT("DA_SoundBank"), Bank))
	{
		return false;
	}
	const TArray<FString> Problems = HollowFeelRules::Check(Config->Entries);
	TestTrue(FString::Printf(TEXT("A sound config (%s)"), *FString::Join(Problems, TEXT("; "))), Problems.IsEmpty());
	TArray<FGameplayTag> Personal = {HollowTags::Feel_Swap, HollowTags::Feel_Damage};
	for (const TCHAR* Path : {NailgunItem, ShotgunItem})
	{
		const UHollowItemDefinition* Weapon = Item(Path);
		const UHollowAbilitySet* Set = Weapon ? Weapon->AbilitySet.LoadSynchronous() : nullptr;
		const FHollowAbilitySetAbility* Fire = Set ? Set->GrantedAbilities.FindByPredicate([](const FHollowAbilitySetAbility& Entry)
		{
			return Entry.Ability && Entry.Ability->IsChildOf<UHollowGA_WeaponFire>();
		}) : nullptr;
		const UHollowWeaponDefinition* Definition = Fire ? Fire->Ability->GetDefaultObject<UHollowGA_WeaponFire>()->GetWeaponDefinition() : nullptr;
		if (TestNotNull(Path, Definition))
		{
			Personal.Add(Definition->FeelTag);
		}
	}
	if (const UClass* Melee = LoadClass<UHollowGA_MeleeBash>(nullptr, MeleeAbility); TestNotNull(TEXT("GA_MeleeBash"), Melee))
	{
		Personal.Add(GetDefault<UHollowGA_MeleeBash>(Melee)->GetFeelTag());
	}
	for (const FGameplayTag& Tag : Personal)
	{
		const FHollowFeelEntry* Entry = Config->Find(Tag);
		TestTrue(Tag.ToString() + TEXT(": one player's own feel"), Tag.IsValid() && Entry && Entry->IsPersonal());
	}
	const UHollowEnemyDefinition* Pilgrim = LoadObject<UHollowEnemyDefinition>(nullptr, TEXT("/Game/Hollow/Characters/Enemies/IronPilgrim/DA_Enemy_IronPilgrim.DA_Enemy_IronPilgrim"));
	TArray<FGameplayTag> World = {HollowTags::Noise_Structural_Breach, HollowTags::Sound_Mine_Blast, HollowTags::Noise_Impact_Crate};
	if (TestNotNull(TEXT("DA_Enemy_IronPilgrim"), Pilgrim))
	{
		World.Add(Pilgrim->FootstepSound);
	}
	for (const FGameplayTag& Tag : World)
	{
		TestTrue(Tag.ToString() + TEXT(": felt round it"), Config->Find(Tag) && !Config->Find(Tag)->IsPersonal());
	}
	for (const FHollowFeelEntry& Entry : Config->Entries)
	{
		TestTrue(Entry.Trigger.ToString() + TEXT(": a sound that plays"), Entry.IsPersonal() || Bank->Find(Entry.Trigger) != nullptr);
	}
	TestTrue(TEXT("Reduced flashes soften it, never more"), Config->ReducedFlashesScale > 0.f && Config->ReducedFlashesScale < 1.f);
	TestFalse(TEXT("The view feel never ticks"), GetDefault<UHollowViewFeelComponent>()->PrimaryComponentTick.bCanEverTick);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowFeelShotsTest, "Hollow.Game.Feel.Shots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowFeelShotsTest::RunTest(const FString& Parameters)
{
	// GP-01c: the shooter feels its swap, its shots and its blow (kick, shake) and sees its hit marker at once: the
	// nailgun clanks off a Pilgrim's plate, a crit once it is exposed, a plain hit on a crate, nothing on a squad mate;
	// the first-person tool follows the kick; reduced flashes soften it.
	using namespace HollowWeaponGameTests;
	FRig Rig;
	if (!Build(*this, Rig))
	{
		return false;
	}
	UHollowFeelSubsystem* Feel = UHollowFeelSubsystem::Get(Rig.Scene.World);
	APlayerController* Player = Cast<APlayerController>(Rig.Character->GetController());
	UHollowViewFeelModifier* View = Feel && Player ? Feel->GetViewModifier(*Player) : nullptr;
	Rig.Inventory->AddItem(Item(NailgunItem), 1);
	Rig.Inventory->AddItem(Item(NailsItem), NailsCarried);
	AHollowEnemyCharacter* Pilgrim = SpawnStillPilgrim(*this, Rig, EnemyAt);
	if (!TestNotNull(TEXT("The feel subsystem"), Feel) || !TestNotNull(TEXT("The player's view feel"), View) || !Pilgrim)
	{
		return false;
	}
	TestTrue(TEXT("The body follows its player's view"), Rig.Character->GetViewFeel()->IsBound());
	Feel->ResetCounts();
	DrawWeapon(Rig);
	TestEqual(TEXT("The swap dips the view"), Feel->GetFeltCount(HollowTags::Feel_Swap), 1);

	const UHollowWeaponDefinition* Weapon = Rig.Hands->GetWeapon();
	AimAt(Rig, *Pilgrim);
	Press(Rig, HollowTags::Input_PrimaryUse);
	TestEqual(TEXT("A shot: its feel"), Feel->GetFeltCount(Weapon->FeelTag), 1);
	TestEqual(TEXT("...the nailgun clanks off the plate"), Rig.Hands->GetLastHitMarker(), EHollowHitKind::Armor);
	TestTrue(TEXT("...and kicks the view"), View->GetKickCount() > 0);
	View->Step(1.f / 60.f, Player->GetControlRotation());
	TestFalse(TEXT("...the tool in the hands with it"), Rig.Character->GetToolMount()->GetViewFeelOffset().Equals(FTransform::Identity));
	Advance(Rig, Weapon->FireIntervalSeconds + 0.05f);

	if (const TSubclassOf<UGameplayEffect> Expose = LoadClass<UGameplayEffect>(nullptr, ExposeEffect); TestNotNull(TEXT("GE_Occult_Expose"), Expose.Get()))
	{
		UAbilitySystemComponent* Target = Pilgrim->GetAbilitySystemComponent();
		Target->ApplyGameplayEffectToSelf(Expose.GetDefaultObject(), 1.f, Target->MakeEffectContext());
		AimAt(Rig, *Pilgrim);
		Press(Rig, HollowTags::Input_PrimaryUse);
		TestEqual(TEXT("Exposed: a crit"), Rig.Hands->GetLastHitMarker(), EHollowHitKind::Crit);
		Advance(Rig, Weapon->FireIntervalSeconds + 0.05f);
	}

	AHollowExoFrameCharacter* Mate = HollowGameTests::SpawnExoFrame(*this, Rig.Scene.World, FTransform(CloseEnemyAt + FVector(0.0, 300.0, 0.0)));
	const int32 Markers = Rig.Hands->GetHitMarkerCount();
	if (Mate)
	{
		AimAt(Rig, *Mate);
		Press(Rig, HollowTags::Input_PrimaryUse);
		TestEqual(TEXT("A squad mate: no marker"), Rig.Hands->GetHitMarkerCount(), Markers);
		Advance(Rig, Weapon->FireIntervalSeconds + 0.05f);
		Mate->Destroy();
	}
	const TSubclassOf<AActor> CrateClass = LoadClass<AActor>(nullptr, TEXT("/Game/Hollow/World/Loot/BP_LootCrate_Docks.BP_LootCrate_Docks_C"));
	AActor* Crate = CrateClass ? Rig.Scene.World->SpawnActor<AActor>(CrateClass, FTransform(CloseEnemyAt + FVector(0.0, -300.0, -70.0))) : nullptr;
	if (TestNotNull(TEXT("A loot crate"), Crate))
	{
		AimAt(Rig, *Crate);
		Press(Rig, HollowTags::Input_PrimaryUse);
		TestEqual(TEXT("A crate: a plain hit"), Rig.Hands->GetLastHitMarker(), EHollowHitKind::Hit);
		Advance(Rig, Weapon->FireIntervalSeconds + 0.05f);
	}

	Pilgrim->SetActorLocation(ShooterAt + FVector(110.0, 0.0, Pilgrim->GetActorLocation().Z - ShooterAt.Z));
	Advance(Rig, 0.8f);
	AimAt(Rig, *Pilgrim);
	Press(Rig, HollowTags::Input_Melee);
	TestEqual(TEXT("A blow: its lunge"), Feel->GetFeltCount(HollowTags::Feel_Melee), 1);
	TestEqual(TEXT("...on the exposed Pilgrim, a crit"), Rig.Hands->GetLastHitMarker(), EHollowHitKind::Crit);

	if (UHollowGameUserSettings* Settings = UHollowGameUserSettings::Get(); TestNotNull(TEXT("The game's settings class"), Settings))
	{
		const bool bBefore = Settings->IsReducedFlashes();
		Settings->SetReducedFlashes(true, /*bSave=*/false);
		Advance(Rig, Weapon->FireIntervalSeconds + 0.05f);
		AimAt(Rig, *Pilgrim);
		Press(Rig, HollowTags::Input_PrimaryUse);
		TestEqual(TEXT("Reduced flashes: a softer kick"), Feel->GetLastScale(), Feel->GetConfig()->ReducedFlashesScale, 1.e-3f);
		Settings->SetReducedFlashes(bBefore, /*bSave=*/false);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowFeelDamageTest, "Hollow.Game.Feel.Damage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowFeelDamageTest::RunTest(const FString& Parameters)
{
	// GP-01c: the player's body jolts as it loses health, the harder the more it lost (full at the component's share),
	// never as it heals; someone else's body jolts nobody here.
	using namespace HollowWeaponGameTests;
	FRig Rig;
	if (!Build(*this, Rig))
	{
		return false;
	}
	UHollowFeelSubsystem* Feel = UHollowFeelSubsystem::Get(Rig.Scene.World);
	const UHollowViewFeelComponent* Body = Rig.Character->GetViewFeel();
	if (!TestNotNull(TEXT("The feel subsystem"), Feel) || !TestTrue(TEXT("The body follows its player"), Body && Body->IsBound()))
	{
		return false;
	}
	const float Max = Rig.AbilitySystem->GetNumericAttribute(UHollowHealthSet::GetMaxHealthAttribute());
	auto SetHealth = [&Rig](UAbilitySystemComponent& AbilitySystem, const float Value)
	{
		UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
		Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
		FGameplayModifierInfo& Modifier = Effect->Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = UHollowHealthSet::GetHealthAttribute();
		Modifier.ModifierOp = EGameplayModOp::Override;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Value));
		AbilitySystem.ApplyGameplayEffectToSelf(Effect, 1.f, AbilitySystem.MakeEffectContext());
	};
	Feel->ResetCounts();
	SetHealth(*Rig.AbilitySystem, Max * 0.9f);
	TestEqual(TEXT("A tenth lost: a jolt"), Feel->GetFeltCount(HollowTags::Feel_Damage), 1);
	TestEqual(TEXT("...as strong as its share says"), Feel->GetLastScale(), Body->DamageFeelScale(Max * 0.1f, Max) * Feel->GetComfortScale(), 1.e-3f);
	SetHealth(*Rig.AbilitySystem, Max * 0.4f);
	TestEqual(TEXT("Half lost at once: the strongest"), Feel->GetLastScale(), Feel->GetComfortScale(), 1.e-3f);
	SetHealth(*Rig.AbilitySystem, Max);
	TestEqual(TEXT("Healing: nothing"), Feel->GetFeltCount(HollowTags::Feel_Damage), 2);

	AHollowExoFrameCharacter* Other = nullptr;
	UHollowAbilitySystemComponent* OtherAbilities = Rig.Scene.SpawnPlayer(*this, ShooterAt + FVector(0.0, 300.0, 0.0), Other);
	if (TestNotNull(TEXT("Another player"), OtherAbilities))
	{
		TestFalse(TEXT("...not followed on this machine"), Other->GetViewFeel()->IsBound());
		SetHealth(*OtherAbilities, Max * 0.5f);
		TestEqual(TEXT("...its damage jolts nobody here"), Feel->GetFeltCount(HollowTags::Feel_Damage), 2);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
