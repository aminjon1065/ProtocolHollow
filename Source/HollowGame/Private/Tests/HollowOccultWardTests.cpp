#include "Tests/HollowEnemyTestScene.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/HollowAbilitySet.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/HollowResonanceSet.h"
#include "Audio/HollowSoundBank.h"
#include "Classes/HollowClassDefinition.h"
#include "Components/DecalComponent.h"
#include "Crafting/HollowCraftingRules.h"
#include "Effects/HollowStatusEffects.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Gadgets/HollowGA_OccultWard.h"
#include "Gadgets/HollowOccultWard.h"
#include "Gadgets/HollowOccultWardDefinition.h"
#include "Gadgets/HollowPulseMine.h"
#include "Inventory/HollowInventoryComponent.h"
#include "Items/HollowItemDefinition.h"
#include "Materials/Material.h"
#include "Net/UnrealNetwork.h"
#include "Profile/HollowProfileTypes.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace HollowOccultWardTests
{
	using namespace HollowEnemyTests;

	// Content under test (create_ward_assets.py, create_gadget_assets.py, create_economy_assets.py, create_class_assets.py).
	const TCHAR* const WardAbilityClass = TEXT("/Game/Hollow/Gadgets/OccultWard/GA_OccultWard.GA_OccultWard_C");
	const TCHAR* const WardClass = TEXT("/Game/Hollow/Gadgets/OccultWard/BP_OccultWard.BP_OccultWard_C");
	const TCHAR* const WardItemPath = TEXT("/Game/Hollow/Items/Consumables/DA_Item_OccultWard.DA_Item_OccultWard");
	const TCHAR* const MineAbilityClass = TEXT("/Game/Hollow/Gadgets/PulseMine/GA_PulseMine.GA_PulseMine_C");
	const TCHAR* const MineItemPath = TEXT("/Game/Hollow/Items/Consumables/DA_Item_PulseMine.DA_Item_PulseMine");
	const FPrimaryAssetId WardItem(UHollowItemDefinition::AssetType, TEXT("DA_Item_OccultWard"));
	const FPrimaryAssetId WardRecipe(TEXT("HollowRecipe"), TEXT("DA_Recipe_OccultWard"));
	const FPrimaryAssetId Occultist(UHollowClassDefinition::AssetType, TEXT("DA_Class_Occultist"));

	// Test data: where the second ward goes (from the first), who stands where around it.
	const FVector Step(0.0, 300.0, 0.0);
	const FVector InsideOffset(150.0, 0.0, 150.0);
	const FVector OutsideOffset(600.0, 0.0, 150.0);
	const FVector FarOffset(900.0, 0.0, 150.0);
	constexpr float StartResonance = 10.f;
	constexpr double FloorSlack = 5.0;
	constexpr int32 WardsTaken = 2;
	constexpr int32 MinesTaken = 1;
	constexpr float DrainedSeconds = 2.f;
	constexpr float SpeedTolerance = 0.01f;

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

	TArray<AHollowOccultWard*> Wards(UWorld* World)
	{
		TArray<AHollowOccultWard*> Found;
		for (TActorIterator<AHollowOccultWard> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				Found.Add(*It);
			}
		}
		return Found;
	}

	float Seconds(const TSubclassOf<UGameplayEffect>& Effect)
	{
		float Value = 0.f;
		return Effect && Effect.GetDefaultObject()->DurationMagnitude.GetStaticMagnitudeIfPossible(1.f, Value) ? Value : 0.f;
	}

	bool IsWarded(const AActor* Actor)
	{
		const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
		return AbilitySystem && AbilitySystem->HasMatchingGameplayTag(HollowTags::State_Warded);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowGadgetOccultWardTest, "Hollow.Game.Gadget.OccultWard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowGadgetOccultWardTest::RunTest(const FString& Parameters)
{
	// ADR-0036, S6-03 acceptance: the Gadget input lays the ward the player brought on the floor at its feet; its
	// cooldown blocks every gadget (no mine meanwhile); one ward per player. Inside the circle an enemy is slowed (not one
	// outside; out again, its speed comes back), a player's Resonance drains and builds slower (not a teammate's far
	// off). Every machine's look follows the replicated time it was laid: it comes up, burns, pulses; then it is gone.
	using namespace HollowOccultWardTests;
	FEnemyScene Scene;
	const TSubclassOf<UGameplayAbility> WardAbility = LoadClass<UGameplayAbility>(nullptr, WardAbilityClass);
	const TSubclassOf<UGameplayAbility> MineAbility = LoadClass<UGameplayAbility>(nullptr, MineAbilityClass);
	UHollowItemDefinition* WardItemDefinition = LoadObject<UHollowItemDefinition>(nullptr, WardItemPath);
	UHollowItemDefinition* MineItemDefinition = LoadObject<UHollowItemDefinition>(nullptr, MineItemPath);
	if (!TestNotNull(TEXT("GA_OccultWard"), WardAbility.Get()) || !TestNotNull(TEXT("GA_PulseMine"), MineAbility.Get())
		|| !TestNotNull(TEXT("DA_Item_OccultWard"), WardItemDefinition) || !TestNotNull(TEXT("DA_Item_PulseMine"), MineItemDefinition)
		|| !Scene.Create(*this, EWall::None))
	{
		return false;
	}
	AHollowExoFrameCharacter* Player = nullptr;
	UHollowAbilitySystemComponent* Abilities = Scene.SpawnPlayer(*this, RoomA, Player, 0.f, /*bLocalPlayer=*/true);
	UHollowInventoryComponent* Inventory = Player ? UHollowInventoryComponent::Get(Player->GetPlayerState()) : nullptr;
	if (!Abilities || !TestNotNull(TEXT("Raid inventory"), Inventory))
	{
		return false;
	}
	Inventory->AddItem(WardItemDefinition, WardsTaken);   // the items grant GA_OccultWard and GA_PulseMine
	Inventory->AddItem(MineItemDefinition, MinesTaken);
	Advance(Scene.TestWorld, StepSeconds * 0.1f);   // on the floor

	// Laid; its cooldown blocks the mine too.
	TestTrue(TEXT("Laid"), Abilities->TryActivateAbilityByClass(WardAbility));
	TArray<AHollowOccultWard*> Laid = Wards(Scene.World);
	if (!TestEqual(TEXT("One ward"), Laid.Num(), 1))
	{
		return false;
	}
	AHollowOccultWard* First = Laid[0];
	const UHollowOccultWardDefinition* Definition = First->GetDefinition();
	TestTrue(TEXT("...on the floor at the feet, the player's"), FMath::Abs(First->GetActorLocation().Z) < FloorSlack
		&& FVector::Dist2D(First->GetActorLocation(), Player->GetActorLocation()) < FloorSlack && First->GetInstigator() == Player);
	TestTrue(TEXT("...coming up"), First->GetGlowAlpha() < 1.f && First->GetSecondsLeft() > Definition->LifeSeconds - FloorSlack);
	TestTrue(TEXT("Its cooldown"), Abilities->HasMatchingGameplayTag(HollowTags::Cooldown_Gadget_OccultWard));
	TestFalse(TEXT("...blocks every gadget: no mine meanwhile"), Abilities->TryActivateAbilityByClass(MineAbility));
	int32 MinesLaid = 0;
	for (TActorIterator<AHollowPulseMine> It(Scene.World); It; ++It)
	{
		++MinesLaid;
	}
	TestTrue(TEXT("...none laid, none spent"), MinesLaid == 0 && Inventory->GetCount(MineItemDefinition) == MinesTaken);

	// One ward per player.
	Abilities->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(HollowTags::Cooldown_Gadget_OccultWard));
	Player->SetActorLocation(Player->GetActorLocation() + Step);
	TestTrue(TEXT("After the cooldown: laid again"), Abilities->TryActivateAbilityByClass(WardAbility));
	Laid = Wards(Scene.World);
	TestTrue(TEXT("One ward per player: the old one is taken back"), !IsValid(First) && Laid.Num() == 1);
	TestEqual(TEXT("Both wards laid"), Inventory->GetCount(WardItemDefinition), 0);
	AHollowOccultWard* Ward = Laid.Num() == 1 ? Laid[0] : nullptr;
	if (!TestNotNull(TEXT("The new ward"), Ward))
	{
		return false;
	}

	// Who is inside.
	const FVector At = Ward->GetActorLocation();
	AHollowEnemyCharacter* Bound = Scene.SpawnPilgrim(*this, At + InsideOffset);
	AHollowEnemyCharacter* Free = Scene.SpawnPilgrim(*this, At + OutsideOffset);
	AHollowExoFrameCharacter* Mate = nullptr;
	UHollowAbilitySystemComponent* MateAbilities = Scene.SpawnPlayer(*this, At + FarOffset, Mate);
	if (!Bound || !Free || !MateAbilities)
	{
		return false;
	}
	AHollowAIController* BoundBrain = Cast<AHollowAIController>(Bound->GetController());
	for (const AHollowEnemyCharacter* Enemy : {Bound, Free})
	{
		Cast<AHollowAIController>(Enemy->GetController())->Shutdown();   // they stand where they are put
	}
	const float Walk = Bound->GetCharacterMovement()->MaxWalkSpeed;
	const FGameplayAttribute Resonance = UHollowResonanceSet::GetResonanceAttribute();
	const FGameplayAttribute Gain = UHollowResonanceSet::GetResonanceGainMultiplierAttribute();
	const float FullGain = MateAbilities->GetNumericAttribute(Gain);
	SetAttribute(Abilities, Resonance, StartResonance);
	SetAttribute(MateAbilities, Resonance, StartResonance);
	Ward->CheckInside();
	TestTrue(TEXT("Inside: the Pilgrim and the placer"), Ward->GetEnemiesInside().Num() == 1 && Ward->GetEnemiesInside()[0] == Bound
		&& Ward->GetPlayersInside().Num() == 1 && Ward->GetPlayersInside()[0] == Player);
	TestTrue(FString::Printf(TEXT("The Pilgrim inside: warded, slowed (x%.2f)"), BoundBrain->GetMoveSpeedMultiplier()), IsWarded(Bound)
		&& BoundBrain->GetMoveSpeedMultiplier() < 1.f && FMath::IsNearlyEqual(Bound->GetCharacterMovement()->MaxWalkSpeed, Walk * BoundBrain->GetMoveSpeedMultiplier(), Walk * SpeedTolerance));
	TestFalse(TEXT("...not the one outside"), IsWarded(Free));
	const float BeforeDrain = Abilities->GetNumericAttribute(Resonance);
	Advance(Scene.TestWorld, DrainedSeconds);
	const float Drained = BeforeDrain - Abilities->GetNumericAttribute(Resonance);
	const float Expected = Definition->ResonanceDrainPerSecond * DrainedSeconds;
	TestTrue(FString::Printf(TEXT("The placer inside: Resonance drains (%.2f in %.0f s, about %.2f)"), Drained, DrainedSeconds, Expected),
		FMath::IsNearlyEqual(Drained, Expected, Definition->ResonanceDrainPerSecond * Definition->CheckSeconds * 2.f));
	TestTrue(TEXT("...and builds slower"), IsWarded(Player) && Abilities->GetNumericAttribute(Gain) < FullGain);
	TestTrue(TEXT("Not the teammate far off"), !IsWarded(Mate) && FMath::IsNearlyEqual(MateAbilities->GetNumericAttribute(Resonance), StartResonance)
		&& FMath::IsNearlyEqual(MateAbilities->GetNumericAttribute(Gain), FullGain));
	TestTrue(TEXT("Burning steady"), FMath::IsNearlyEqual(Ward->GetGlowAlpha(), 1.f));

	// Out of the circle, the Pilgrim's speed comes back once the bind lingers out.
	Bound->SetActorLocation(At + FarOffset + FarOffset);
	Advance(Scene.TestWorld, Seconds(Definition->EnemyEffect) + Definition->CheckSeconds * 2.f);
	TestTrue(TEXT("Out again: its speed back"), !IsWarded(Bound) && FMath::IsNearlyEqual(Bound->GetCharacterMovement()->MaxWalkSpeed, Walk));

	// The end: it pulses, then it is gone.
	Advance(Scene.TestWorld, Definition->LifeSeconds, [Ward, Definition] { return Ward->GetSecondsLeft() < Definition->PulseSeconds * 0.5f; });
	const float PulseA = Ward->GetGlowAlpha();
	Advance(Scene.TestWorld, 0.5f / Definition->PulseHz);
	TestTrue(FString::Printf(TEXT("It pulses before it goes out (%.2f, then %.2f)"), PulseA, Ward->GetGlowAlpha()), !FMath::IsNearlyEqual(PulseA, Ward->GetGlowAlpha(), 0.1f));
	Advance(Scene.TestWorld, Definition->LifeSeconds, [Ward] { return !IsValid(Ward); });
	TestTrue(TEXT("After LifeSeconds it is gone"), !IsValid(Ward) && Wards(Scene.World).IsEmpty());

	// What every machine gets: the actor and the time it was laid, once.
	const AHollowOccultWard* Defaults = GetDefault<AHollowOccultWard>();
	TArray<FLifetimeProperty> Replicated;
	static_cast<const UObject*>(Defaults)->GetLifetimeReplicatedProps(Replicated);   // protected on the actor, public on UObject
	const FProperty* Placed = AHollowOccultWard::StaticClass()->FindPropertyByName(TEXT("PlacedServerTime"));
	TestTrue(TEXT("It replicates its laying time, initial only"), Defaults->GetIsReplicated() && Placed && Replicated.ContainsByPredicate([Placed](const FLifetimeProperty& Each)
	{
		return Each.RepIndex == Placed->RepIndex && Each.Condition == COND_InitialOnly;
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowOccultWardDataTest, "Hollow.Game.OccultWard.Data",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowOccultWardDataTest::RunTest(const FString& Parameters)
{
	// The shipped ward (create_ward_assets.py): a valid definition behind BP_OccultWard and its rune decal, the ability
	// with its cooldown on the Gadget input of a consumable item, two in the Occultist's starting loadout, a workbench
	// recipe; its hum is a players' sound the AI does not hear.
	using namespace HollowOccultWardTests;
	const TSubclassOf<AHollowOccultWard> Ward = LoadClass<AHollowOccultWard>(nullptr, WardClass);
	const TSubclassOf<UGameplayAbility> Ability = LoadClass<UGameplayAbility>(nullptr, WardAbilityClass);
	const UHollowItemDefinition* Item = LoadObject<UHollowItemDefinition>(nullptr, WardItemPath);
	const AHollowOccultWard* WardDefaults = Ward ? Ward.GetDefaultObject() : nullptr;
	const UHollowOccultWardDefinition* Definition = WardDefaults ? WardDefaults->GetDefinition() : nullptr;
	if (!TestNotNull(TEXT("BP_OccultWard"), WardDefaults) || !TestNotNull(TEXT("DA_OccultWard"), Definition) || !TestNotNull(TEXT("GA_OccultWard"), Ability.Get())
		|| !TestNotNull(TEXT("DA_Item_OccultWard"), Item))
	{
		return false;
	}
#if WITH_EDITOR
	FDataValidationContext Validation;
	TestTrue(TEXT("The ward definition is valid"), Definition->IsDataValid(Validation) != EDataValidationResult::Invalid && Validation.GetNumErrors() == 0);
#endif
	const UMaterialInterface* Rune = WardDefaults->GetRune()->GetDecalMaterial();
	TestTrue(TEXT("Its look: a rune decal"), Rune && Rune->GetMaterial() && Rune->GetMaterial()->MaterialDomain == MD_DeferredDecal);
	const UHollowGameplayAbility* AbilityDefaults = Cast<UHollowGameplayAbility>(Ability.GetDefaultObject());
	TestTrue(TEXT("The ability: a gadget with its own cooldown, one ward a use"), AbilityDefaults && AbilityDefaults->GetAssetTags().HasTag(HollowTags::Ability_Gadget_OccultWard)
		&& AbilityDefaults->GetCooldownGameplayEffect() && AbilityDefaults->GetCooldownGameplayEffect()->IsA<UHollowOccultWardCooldownEffect>()
		&& AbilityDefaults->GetItemCostPerUse() == 1);
	TestTrue(TEXT("The item: a consumable, up to three a slot"), Item->ItemType == HollowTags::Item_Type_Consumable && Item->MaxStack == 3 && !Item->bInsurable);
	const UHollowAbilitySet* Set = Item->AbilitySet.LoadSynchronous();
	TestTrue(TEXT("...granting the ward on the Gadget input"), Set && Set->GrantedAbilities.Num() == 1 && Set->GrantedAbilities[0].Ability == Ability
		&& Set->GrantedAbilities[0].InputTag == HollowTags::Input_Gadget);

	const FHollowEconomyTerms Terms = FHollowEconomyTerms::Load(FHollowProfileState());
	const FHollowClassTerms* Class = Terms.Classes.Find(Occultist);
	TestTrue(TEXT("Two in the Occultist's starting loadout"), Class && Class->StartingLoadout.ContainsByPredicate([](const FHollowLoadoutSlot& Slot)
	{
		return Slot.Item == WardItem && Slot.Count == 2;
	}));
	const FHollowRecipeTerms* Recipe = Terms.Recipes.Find(WardRecipe);
	TestTrue(TEXT("Made on the workbench"), Recipe && Recipe->Output.Item == WardItem && Recipe->Output.Count > 0);
	TestFalse(TEXT("...not sold in the shop (consumables come from the workbench)"), Terms.ShopPrices.Contains(WardItem));

	const UHollowSoundBank* Bank = UHollowSoundBank::Get();
	const FHollowSoundEntry* Hum = Bank ? Bank->Find(Definition->HumSound) : nullptr;
	TestTrue(TEXT("Its hum: a players' sound in the bank, no noise for the AI"), Definition->HumSound == HollowTags::Sound_Ward_Hum && Hum && !Hum->Noise.IsValid());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
