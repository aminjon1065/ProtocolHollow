#include "Tests/HollowGameTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/GameplayAbility.h"
#include "Abilities/HollowAbilitySet.h"
#include "Abilities/HollowGameplayAbility.h"
#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Attributes/HollowToolSet.h"
#include "EngineUtils.h"
#include "Framework/HollowGameState.h"
#include "Gadgets/HollowPulseMine.h"
#include "GameplayEffect.h"
#include "Inventory/HollowInventoryComponent.h"
#include "Inventory/HollowLoadoutRules.h"
#include "Raid/HollowRaidFlowComponent.h"
#include "Raid/HollowRaidStateComponent.h"
#include "Tags/HollowGameplayTags.h"

namespace HollowLoadoutSyncTests
{
	const TCHAR* const InitEffectClass = TEXT("/Game/Hollow/GAS/Effects/GE_ExoFrame_InitAttributes.GE_ExoFrame_InitAttributes_C");
	const TCHAR* const SuitSetPath = TEXT("/Game/Hollow/Characters/ExoFrame/DA_AbilitySet_ExoFrame.DA_AbilitySet_ExoFrame");
	const TCHAR* const WelderAbilityClass = TEXT("/Game/Hollow/Tools/Welder/GA_Tool_Welder.GA_Tool_Welder_C");
	const TCHAR* const CutterAbilityClass = TEXT("/Game/Hollow/Tools/PlasmaCutter/GA_Tool_PlasmaCutter.GA_Tool_PlasmaCutter_C");
	const TCHAR* const MineAbilityClass = TEXT("/Game/Hollow/Gadgets/PulseMine/GA_PulseMine.GA_PulseMine_C");
	const TCHAR* const RefuelAbilityClass = TEXT("/Game/Hollow/Tools/Refuel/GA_ToolRefuel.GA_ToolRefuel_C");
	const FPrimaryAssetId Welder(TEXT("HollowItem"), TEXT("DA_Item_Welder"));
	const FPrimaryAssetId Cutter(TEXT("HollowItem"), TEXT("DA_Item_PlasmaCutter"));
	const FPrimaryAssetId FuelCell(TEXT("HollowItem"), TEXT("DA_Item_FuelCell"));
	const FPrimaryAssetId PulseMine(TEXT("HollowItem"), TEXT("DA_Item_PulseMine"));
	const FPrimaryAssetId Core(TEXT("HollowItem"), TEXT("DA_Item_AetherCore"));
	const FPrimaryAssetId Unknown(TEXT("HollowItem"), TEXT("Test_NoSuchItem"));
	constexpr int32 Cells = 3;
	constexpr int32 Mines = 2;
	/** Fuel left in the tank before the first refuel (of 100). */
	constexpr float LowFuel = 20.f;

	FHollowItemStack Stack(const FPrimaryAssetId& Item, const int32 Count)
	{
		FHollowItemStack Result;
		Result.Item = Item;
		Result.Count = Count;
		return Result;
	}

	FHollowLoadoutSnapshot Snapshot(const FGuid& RaidId, TArray<FHollowItemStack> Slots)
	{
		FHollowLoadoutSnapshot Result;
		Result.RaidId = RaidId;
		Result.Slots = MoveTemp(Slots);
		return Result;
	}

	/** A local player's ExoFrame with its starting attributes (local: its predicted abilities run on this machine). */
	APlayerController* SpawnPlayer(FAutomationTestBase& Test, UWorld* World, const FVector& Location, const TSubclassOf<UGameplayEffect>& Init)
	{
		AHollowExoFrameCharacter* Character = HollowGameTests::SpawnExoFrame(Test, World, FTransform(Location));
		if (!Character)
		{
			return nullptr;
		}
		APlayerController* Controller = HollowGameTests::PossessAsPlayer(World, Character, /*bLocalPlayer=*/true);
		UHollowAbilitySystemComponent* AbilitySystem = Character->GetHollowAbilitySystemComponent();
		if (AbilitySystem)
		{
			AbilitySystem->ApplyGameplayEffectToSelf(Init.GetDefaultObject(), 1.f, AbilitySystem->MakeEffectContext());
		}
		return Controller;
	}

	UHollowAbilitySystemComponent* AbilitiesOf(const APlayerController* Controller)
	{
		const AHollowPlayerState* State = Controller ? Controller->GetPlayerState<AHollowPlayerState>() : nullptr;
		return State ? State->GetHollowAbilitySystemComponent() : nullptr;
	}

	UHollowInventoryComponent* InventoryOf(const APlayerController* Controller)
	{
		return Controller ? UHollowInventoryComponent::Get(Controller->PlayerState) : nullptr;
	}

	/** The spec of AbilityClass granted on Input, if any. */
	const FGameplayAbilitySpec* FindSpec(const UAbilitySystemComponent* AbilitySystem, const UClass* AbilityClass, const FGameplayTag& Input)
	{
		for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
		{
			if (Spec.Ability && Spec.Ability->GetClass() == AbilityClass && Spec.GetDynamicSpecSourceTags().HasTagExact(Input))
			{
				return &Spec;
			}
		}
		return nullptr;
	}

	void SetFuel(UAbilitySystemComponent& AbilitySystem, const float Value)
	{
		UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
		Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
		FGameplayModifierInfo& Modifier = Effect->Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = UHollowToolSet::GetToolFuelAttribute();
		Modifier.ModifierOp = EGameplayModOp::Override;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Value));
		AbilitySystem.ApplyGameplayEffectToSelf(Effect, 1.f, AbilitySystem.MakeEffectContext());
	}

	int32 MinesLaid(UWorld* World)
	{
		int32 Count = 0;
		for (TActorIterator<AHollowPulseMine> It(World); It; ++It)
		{
			Count += IsValid(*It) ? 1 : 0;
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowLoadoutSyncRaidTest, "Hollow.Economy.LoadoutSync.Raid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowLoadoutSyncRaidTest::RunTest(const FString& Parameters)
{
	// S5-01 (ADR-0030) on the host: the raid takes each player's loadout once, for this raid, while it assembles; what the
	// rules accept becomes the raid inventory, whose items alone grant the tools and consumables (the suit grants
	// none). A player without mines cannot lay one; mines and fuel cells run out; what was used up is what the result
	// reports as spent. A loadout after the raid began is refused.
	using namespace HollowLoadoutSyncTests;
	const TSubclassOf<UGameplayEffect> Init = LoadClass<UGameplayEffect>(nullptr, InitEffectClass);
	const UHollowAbilitySet* SuitSet = LoadObject<UHollowAbilitySet>(nullptr, SuitSetPath);
	const TSubclassOf<UGameplayAbility> WelderAbility = LoadClass<UGameplayAbility>(nullptr, WelderAbilityClass);
	const TSubclassOf<UGameplayAbility> CutterAbility = LoadClass<UGameplayAbility>(nullptr, CutterAbilityClass);
	const TSubclassOf<UGameplayAbility> MineAbility = LoadClass<UGameplayAbility>(nullptr, MineAbilityClass);
	const TSubclassOf<UGameplayAbility> RefuelAbility = LoadClass<UGameplayAbility>(nullptr, RefuelAbilityClass);
	FTestWorldWrapper TestWorld;
	UWorld* World = HollowGameTests::CreateWorld(*this, TestWorld);
	if (!World || !TestNotNull(TEXT("GE_ExoFrame_InitAttributes"), Init.Get()) || !TestNotNull(TEXT("DA_AbilitySet_ExoFrame"), SuitSet)
		|| !TestNotNull(TEXT("GA_Tool_Welder"), WelderAbility.Get()) || !TestNotNull(TEXT("GA_Tool_PlasmaCutter"), CutterAbility.Get())
		|| !TestNotNull(TEXT("GA_PulseMine"), MineAbility.Get()) || !TestNotNull(TEXT("GA_ToolRefuel"), RefuelAbility.Get()))
	{
		return false;
	}
	for (const FHollowAbilitySetAbility& Entry : SuitSet->GrantedAbilities)
	{
		TestTrue(TEXT("The suit grants no tool or consumable: ") + GetNameSafe(Entry.Ability.Get()), Entry.Ability != WelderAbility && Entry.Ability != CutterAbility
			&& Entry.Ability != MineAbility && Entry.Ability != RefuelAbility);
	}

	// The raid's game state and flow (test worlds have no game mode: the flow on its own actor), assembling.
	AHollowGameState* GameState = World->SpawnActor<AHollowGameState>();
	GameState->DispatchBeginPlay();
	AActor* Host = World->SpawnActor<AActor>();
	UHollowRaidFlowComponent* Flow = NewObject<UHollowRaidFlowComponent>(Host);
	Flow->RegisterComponent();
	Host->DispatchBeginPlay();
	UHollowRaidStateComponent* RaidState = UHollowRaidStateComponent::Get(World);
	if (!TestNotNull(TEXT("Raid state"), RaidState) || !TestTrue(TEXT("The raid has its id and assembles"), Flow->GetRaidId().IsValid()
		&& RaidState->GetState().Phase == EHollowRaidPhase::Assembling))
	{
		return false;
	}
	// A local player sets up input; the bare C++ class has no InputConfig (BP_ExoFrame data), which it reports.
	AddExpectedMessage(TEXT("InputConfig is not set"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, /*Occurrences=*/0, /*IsRegex=*/false);
	APlayerController* Ann = SpawnPlayer(*this, World, FVector::ZeroVector, Init);
	APlayerController* Bob = SpawnPlayer(*this, World, FVector(0.0, 400.0, 0.0), Init);
	APlayerController* Late = SpawnPlayer(*this, World, FVector(0.0, 800.0, 0.0), Init);
	UHollowAbilitySystemComponent* AnnAbilities = AbilitiesOf(Ann);
	UHollowAbilitySystemComponent* BobAbilities = AbilitiesOf(Bob);
	UHollowInventoryComponent* AnnInventory = InventoryOf(Ann);
	if (!TestTrue(TEXT("Players with ability systems and inventories"), AnnAbilities && BobAbilities && AnnInventory && InventoryOf(Bob) && InventoryOf(Late)))
	{
		return false;
	}
	TestTrue(TEXT("Before the loadout: no tools"), !FindSpec(AnnAbilities, WelderAbility, HollowTags::Input_SecondaryUse)
		&& !AnnAbilities->TryActivateAbilityByClass(MineAbility));

	// Ann's loadout, with an Aether core and an unknown item the raid does not take.
	AddExpectedMessage(TEXT("Unknown item HollowItem:Test_NoSuchItem"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1, /*IsRegex=*/false);
	AddExpectedMessage(TEXT("cut down"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1, /*IsRegex=*/false);
	const FGuid RaidId = Flow->GetRaidId();
	TestTrue(TEXT("Ann's loadout is taken"), Flow->AcceptLoadout(Ann, Snapshot(RaidId, {Stack(Welder, 1), Stack(Cutter, 1), Stack(FuelCell, Cells), Stack(PulseMine, Mines),
		Stack(Core, 1), Stack(Unknown, 1)})));
	TestTrue(TEXT("...her inventory: the four raid items"), AnnInventory->GetEntries().Num() == 4 && AnnInventory->GetCount(PulseMine) == Mines
		&& AnnInventory->GetCount(FuelCell) == Cells && AnnInventory->GetCount(Core) == 0);
	TestTrue(TEXT("...the cutter on primary use, the welder on secondary"), FindSpec(AnnAbilities, CutterAbility, HollowTags::Input_PrimaryUse)
		&& FindSpec(AnnAbilities, WelderAbility, HollowTags::Input_SecondaryUse));
	TestTrue(TEXT("...the mine on the gadget input, the refuel on reload"), FindSpec(AnnAbilities, MineAbility, HollowTags::Input_Gadget)
		&& FindSpec(AnnAbilities, RefuelAbility, HollowTags::Input_Reload));

	AddExpectedMessage(TEXT("ignored: already taken"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1, /*IsRegex=*/false);
	TestFalse(TEXT("Once per raid"), Flow->AcceptLoadout(Ann, Snapshot(RaidId, {Stack(PulseMine, Mines)})));
	TestEqual(TEXT("...her mines unchanged"), AnnInventory->GetCount(PulseMine), Mines);
	AddExpectedMessage(TEXT("ignored: for another raid"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1, /*IsRegex=*/false);
	TestFalse(TEXT("A loadout for another raid is refused"), Flow->AcceptLoadout(Bob, Snapshot(FGuid::NewGuid(), {Stack(PulseMine, Mines)})));
	// Bob brings only the welder, but his ship has the fuel reserve (ADR-0032): two cells on top, free.
	FHollowLoadoutSnapshot BobsLoadout = Snapshot(RaidId, {Stack(Welder, 1)});
	BobsLoadout.Upgrades = {FPrimaryAssetId(TEXT("HollowUpgrade"), TEXT("DA_Upgrade_FuelReserve"))};
	TestTrue(TEXT("Bob takes only the welder"), Flow->AcceptLoadout(Bob, BobsLoadout));
	UHollowInventoryComponent* BobsInventory = InventoryOf(Bob);
	TestTrue(TEXT("...and the ship's two fuel cells, with the refuel"), BobsInventory->GetCount(FuelCell) == 2 && FindSpec(BobAbilities, RefuelAbility, HollowTags::Input_Reload));

	// No mine in the loadout, no mine laid.
	TestTrue(TEXT("Bob has the welder, no mine"), FindSpec(BobAbilities, WelderAbility, HollowTags::Input_SecondaryUse) && !FindSpec(BobAbilities, MineAbility, HollowTags::Input_Gadget));
	TestFalse(TEXT("...so he cannot lay one"), BobAbilities->TryActivateAbilityByClass(MineAbility));
	TestEqual(TEXT("...none on the floor"), MinesLaid(World), 0);

	// Ann's two mines: laid one after the other (cooldown in between), then none left.
	TestTrue(TEXT("Ann lays a mine"), AnnAbilities->TryActivateAbilityByClass(MineAbility) && MinesLaid(World) == 1 && AnnInventory->GetCount(PulseMine) == Mines - 1);
	AnnAbilities->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(HollowTags::Cooldown_Gadget_PulseMine));
	Ann->GetPawn()->SetActorLocation(FVector(300.0, 0.0, 0.0));
	TestTrue(TEXT("...and her second"), AnnAbilities->TryActivateAbilityByClass(MineAbility) && AnnInventory->GetCount(PulseMine) == 0);
	AnnAbilities->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(HollowTags::Cooldown_Gadget_PulseMine));
	TestFalse(TEXT("...no third: none left"), AnnAbilities->TryActivateAbilityByClass(MineAbility));

	// Fuel cells: never into a full tank; one cell per refuel, its cooldown between two.
	const float MaxFuel = AnnAbilities->GetNumericAttribute(UHollowToolSet::GetMaxToolFuelAttribute());
	TestFalse(TEXT("A full tank takes no cell"), AnnAbilities->TryActivateAbilityByClass(RefuelAbility));
	TestEqual(TEXT("...the cells stay"), AnnInventory->GetCount(FuelCell), Cells);
	SetFuel(*AnnAbilities, LowFuel);
	TestTrue(TEXT("Low on fuel: a cell goes in"), AnnAbilities->TryActivateAbilityByClass(RefuelAbility) && AnnInventory->GetCount(FuelCell) == Cells - 1);
	const float Refuelled = AnnAbilities->GetNumericAttribute(UHollowToolSet::GetToolFuelAttribute());
	TestTrue(FString::Printf(TEXT("...the tank fills (%.0f -> %.0f)"), LowFuel, Refuelled), Refuelled > LowFuel && Refuelled <= MaxFuel);
	TestFalse(TEXT("...not again at once"), AnnAbilities->TryActivateAbilityByClass(RefuelAbility));
	AnnAbilities->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(HollowTags::Cooldown_Tool_Refuel));
	TestTrue(TEXT("A second cell tops it up to the full tank"), AnnAbilities->TryActivateAbilityByClass(RefuelAbility)
		&& FMath::IsNearlyEqual(AnnAbilities->GetNumericAttribute(UHollowToolSet::GetToolFuelAttribute()), MaxFuel) && AnnInventory->GetCount(FuelCell) == Cells - 2);

	// What the result reports as spent.
	const TArray<FHollowItemStack> Spent = AnnInventory->GetSpent();
	const auto SpentOf = [&Spent](const FPrimaryAssetId& Item)
	{
		const FHollowItemStack* Found = Spent.FindByPredicate([&Item](const FHollowItemStack& S) { return S.Item == Item; });
		return Found ? Found->Count : 0;
	};
	TestTrue(FString::Printf(TEXT("Spent: both mines, two cells (%s)"), *HollowLoadoutRules::Describe(Spent)), Spent.Num() == 2 && SpentOf(PulseMine) == Mines
		&& SpentOf(FuelCell) == 2);
	SetFuel(*BobAbilities, LowFuel);
	TestTrue(TEXT("Bob refuels with a ship's cell"), BobAbilities->TryActivateAbilityByClass(RefuelAbility) && BobsInventory->GetCount(FuelCell) == 1);
	TestTrue(TEXT("Bob spent nothing of his own"), BobsInventory->GetSpent().IsEmpty());

	// Too late once the raid has begun.
	FHollowRaidNetState InRaid = RaidState->GetState();
	InRaid.Phase = EHollowRaidPhase::InRaid;
	RaidState->SetState(InRaid);
	AddExpectedMessage(TEXT("ignored: the raid has begun"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1, /*IsRegex=*/false);
	TestFalse(TEXT("A loadout after the raid began is refused"), Flow->AcceptLoadout(Late, Snapshot(RaidId, {Stack(PulseMine, Mines)})));
	TestTrue(TEXT("...that player goes without"), InventoryOf(Late)->GetEntries().IsEmpty());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
