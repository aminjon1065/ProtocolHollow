#include "Tests/HollowEnemyTestScene.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/HollowAbilitySet.h"
#include "Components/BoxComponent.h"
#include "Data/HollowBiomeDefinition.h"
#include "Engine/AssetManager.h"
#include "Inventory/HollowInventoryComponent.h"
#include "Items/HollowItemDefinition.h"
#include "Loot/HollowLootTable.h"
#include "Tags/HollowGameplayTags.h"
#include "Weapons/HollowHandsComponent.h"
#include "World/HollowLootCrate.h"
#include "World/HollowLootPickup.h"

namespace HollowLootGameTests
{
	// Content under test (create_loot_assets.py, create_weapon_assets.py, create_art_kit.py).
	const TCHAR* const DocksCrate = TEXT("/Game/Hollow/World/Loot/BP_LootCrate_Docks.BP_LootCrate_Docks_C");
	const TCHAR* const CathedralCrate = TEXT("/Game/Hollow/World/Loot/BP_LootCrate_Cathedral.BP_LootCrate_Cathedral_C");
	const TCHAR* const DocksBiome = TEXT("/HollowSector_Docks/Data/DA_Biome_Docks.DA_Biome_Docks");
	const TCHAR* const CathedralBiome = TEXT("/HollowSector_Cathedral/Data/DA_Biome_Cathedral.DA_Biome_Cathedral");
	const TCHAR* const NailgunItem = TEXT("/Game/Hollow/Items/Weapons/DA_Item_Weapon_Nailgun.DA_Item_Weapon_Nailgun");
	const TCHAR* const SuitSet = TEXT("/Game/Hollow/Characters/ExoFrame/DA_AbilitySet_ExoFrame.DA_AbilitySet_ExoFrame");
	const TCHAR* const FireCue = TEXT("/Game/Hollow/GAS/Cues/GCN_Weapon_Fire.GCN_Weapon_Fire_C");

	/** Test data: the player in room A, a crate ahead of it. */
	const FVector PlayerAt(400.0, 0.0, 100.0);
	const FVector CrateAt(650.0, 0.0, 30.0);
	constexpr int32 Seed = 1234;
	constexpr float SettleSeconds = 2.f;

	AHollowLootCrate* SpawnCrate(const HollowEnemyTests::FEnemyScene& Scene, const TCHAR* ClassPath, const FVector& Location, const int32 CrateSeed)
	{
		const TSubclassOf<AHollowLootCrate> Class = LoadClass<AHollowLootCrate>(nullptr, ClassPath);
		const FTransform Transform(Location);
		AHollowLootCrate* Crate = Class ? Scene.World->SpawnActorDeferred<AHollowLootCrate>(Class, Transform) : nullptr;
		if (Crate)
		{
			Crate->SetSpawnSeed(CrateSeed);   // as the sector's authority does
			Crate->FinishSpawning(Transform);
		}
		return Crate;
	}

	int32 CountOf(const TArray<FHollowItemStack>& Stacks, const FPrimaryAssetId& Item)
	{
		int32 Total = 0;
		for (const FHollowItemStack& Stack : Stacks)
		{
			Total += Stack.Item == Item ? Stack.Count : 0;
		}
		return Total;
	}

	FHollowStructuralHit Blow(const float Amount)
	{
		FHollowStructuralHit Hit;
		Hit.Amount = Amount;
		Hit.Direction = FVector::ForwardVector;
		Hit.DamageType = HollowTags::Damage_Kinetic;
		return Hit;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowLootDataTest, "Hollow.Game.Loot.Data",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowLootDataTest::RunTest(const FString& Parameters)
{
	// GP-01b (ADR-0045): both sectors' Loot sockets spawn their crate first; every item a crate holds lies on the floor
	// as its own mesh.
	using namespace HollowLootGameTests;
	for (const TPair<const TCHAR*, const TCHAR*>& Sector : {TPair<const TCHAR*, const TCHAR*>(DocksBiome, DocksCrate),
		TPair<const TCHAR*, const TCHAR*>(CathedralBiome, CathedralCrate)})
	{
		const UHollowBiomeDefinition* Biome = LoadObject<UHollowBiomeDefinition>(nullptr, Sector.Key);
		const UClass* Crate = LoadClass<AHollowLootCrate>(nullptr, Sector.Value);
		if (!TestNotNull(Sector.Key, Biome) || !TestNotNull(Sector.Value, Crate))
		{
			continue;
		}
		const FHollowSocketActorRule* First = Biome->SocketActors.FindByPredicate([](const FHollowSocketActorRule& Rule)
		{
			return Rule.SocketType == HollowTags::Socket_Loot;
		});
		TestTrue(FString(Sector.Key) + TEXT(": its Loot sockets spawn its crates"), First && First->ActorClass.LoadSynchronous() == Crate && First->MaxPerSector == 0);
	}
	// Every item the tables roll is real and has its pickup mesh.
	TArray<FPrimaryAssetId> Rolled;
	for (const TCHAR* Path : {TEXT("/Game/Hollow/World/Loot/DA_LootTable_Docks.DA_LootTable_Docks"), TEXT("/Game/Hollow/World/Loot/DA_LootTable_Cathedral.DA_LootTable_Cathedral")})
	{
		const UHollowLootTableDefinition* Table = LoadObject<UHollowLootTableDefinition>(nullptr, Path);
		if (!TestNotNull(Path, Table))
		{
			continue;
		}
		TestTrue(FString(Path) + TEXT(": one or two drops a crate"), Table->MinDrops >= 1 && Table->MaxDrops <= 3 && !Table->Rows.IsEmpty());
		for (const FHollowLootRow& Row : Table->Rows)
		{
			Rolled.AddUnique(Row.Item);
		}
	}
	for (const FPrimaryAssetId& Id : Rolled)
	{
		const UHollowItemDefinition* Item = Cast<UHollowItemDefinition>(UAssetManager::Get().GetPrimaryAssetPath(Id).TryLoad());
		TestTrue(Id.ToString() + TEXT(": a real item with its pickup mesh"), Item && !Item->PickupMesh.IsNull());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowLootCrateTest, "Hollow.Game.Loot.Crate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowLootCrateTest::RunTest(const FString& Parameters)
{
	// GP-01b: a crate holds what its seed rolls; blows and the cutter break it (the welder does not), its loot spills out
	// as pickups and the crate no longer blocks; picked up, the loot is the player's found loot.
	using namespace HollowLootGameTests;
	HollowEnemyTests::FEnemyScene Scene;
	if (!Scene.Create(*this, HollowEnemyTests::EWall::None))
	{
		return false;
	}
	AHollowLootCrate* Crate = SpawnCrate(Scene, DocksCrate, CrateAt, Seed);
	AHollowLootCrate* Twin = SpawnCrate(Scene, DocksCrate, CrateAt + FVector(0.0, 300.0, 0.0), Seed);
	AHollowLootCrate* Other = SpawnCrate(Scene, DocksCrate, CrateAt + FVector(0.0, -300.0, 0.0), Seed + 1);
	if (!TestTrue(TEXT("Three crates"), Crate && Twin && Other))
	{
		return false;
	}
	const TArray<FHollowItemStack> Contents = Crate->GetContents();
	TestTrue(TEXT("It holds something"), !Contents.IsEmpty());
	TestTrue(TEXT("The same seed: the same loot"), Contents == Twin->GetContents());

	const float Health = Crate->GetHealth();
	TestTrue(TEXT("A blow dents it"), Crate->ApplyPropHit(Blow(Health * 0.5f)) && !Crate->IsBroken());
	FHollowToolWork Weld;
	Weld.ToolTag = HollowTags::Ability_Tool_Welder;
	Weld.Amount = 1000.f;
	TestFalse(TEXT("The welder does nothing to it"), Crate->CanAcceptToolWork(Weld));
	TestTrue(TEXT("The next blow breaks it"), Crate->ApplyPropHit(Blow(Health)) && Crate->IsBroken());
	TestFalse(TEXT("...once"), Crate->ApplyPropHit(Blow(Health)));
	const UBoxComponent* Bounds = Crate->FindComponentByClass<UBoxComponent>();
	TestTrue(TEXT("Broken: it no longer blocks or shapes navigation"), Bounds && Bounds->GetCollisionEnabled() == ECollisionEnabled::NoCollision
		&& !Bounds->CanEverAffectNavigation());
	TestEqual(TEXT("A pickup per stack"), Crate->GetSpilled().Num(), Contents.Num());

	FHollowToolWork Cut;
	Cut.ToolTag = HollowTags::Ability_Tool_PlasmaCutter;
	Cut.Amount = 1000.f;
	TestTrue(TEXT("The plasma cutter cuts another open"), Other->CanAcceptToolWork(Cut));
	Other->ApplyToolWork(Cut);
	TestTrue(TEXT("...at once with that much work"), Other->IsBroken());

	HollowEnemyTests::Advance(Scene.TestWorld, SettleSeconds);
	AHollowExoFrameCharacter* Player = nullptr;
	if (!Scene.SpawnPlayer(*this, PlayerAt, Player))
	{
		return false;
	}
	UHollowInventoryComponent* Inventory = UHollowInventoryComponent::Get(Player->GetPlayerState());
	if (!TestNotNull(TEXT("The player's raid inventory"), Inventory))
	{
		return false;
	}
	for (const TWeakObjectPtr<AHollowLootPickup>& Weak : Crate->GetSpilled())
	{
		AHollowLootPickup* Pickup = Weak.Get();
		if (!TestNotNull(TEXT("A pickup on the floor"), Pickup))
		{
			continue;
		}
		TestTrue(TEXT("...one of the crate's stacks"), Pickup->GetItem() && CountOf(Contents, Pickup->GetItem()->GetPrimaryAssetId()) == Pickup->GetCount());
		TestTrue(TEXT("...not through the floor"), Pickup->GetActorLocation().Z > -5.0);
		TestTrue(TEXT("...its prompt names it"), Pickup->GetInteractionInfo(Player).Prompt.ToString().Contains(Pickup->GetItem()->DisplayName.ToString()));
		TestTrue(TEXT("...the player may take it"), Pickup->CanInteract(Player));
		Pickup->Interact(Player);
		TestTrue(TEXT("...taken once"), Pickup->IsTaken() && !Pickup->CanInteract(Player));
	}
	const TArray<FHollowItemStack> Found = Inventory->GetFound();
	for (const FHollowItemStack& Stack : Contents)
	{
		TestEqual(Stack.Item.ToString() + TEXT(": the player's found loot"), CountOf(Found, Stack.Item), Stack.Count);
		TestEqual(Stack.Item.ToString() + TEXT(": held"), Inventory->GetCount(Stack.Item), Stack.Count);
	}
	TestTrue(TEXT("Nothing of it counts as spent"), Inventory->GetSpent().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowLootShootTest, "Hollow.Game.Loot.Shoot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowLootShootTest::RunTest(const FString& Parameters)
{
	// GP-01b: the nailgun breaks a crate in a few rounds, the melee blow in a few blows; neither touches a door.
	using namespace HollowLootGameTests;
	HollowEnemyTests::FEnemyScene Scene;
	if (!Scene.Create(*this, HollowEnemyTests::EWall::None) || !TestNotNull(TEXT("GCN_Weapon_Fire"), LoadClass<UObject>(nullptr, FireCue)))
	{
		return false;
	}
	AHollowExoFrameCharacter* Player = nullptr;
	UHollowAbilitySystemComponent* Abilities = Scene.SpawnPlayer(*this, PlayerAt, Player, 0.f, /*bLocalPlayer=*/true);
	const UHollowAbilitySet* Suit = LoadObject<UHollowAbilitySet>(nullptr, SuitSet);
	AHollowLootCrate* Crate = SpawnCrate(Scene, DocksCrate, CrateAt, Seed);
	if (!Abilities || !TestNotNull(TEXT("Suit"), Suit) || !TestNotNull(TEXT("Crate"), Crate))
	{
		return false;
	}
	FHollowAbilitySetHandles Granted;
	Suit->GiveToAbilitySystem(Abilities, &Granted, Player);
	UHollowInventoryComponent::Get(Player->GetPlayerState())->AddItem(LoadObject<UHollowItemDefinition>(nullptr, NailgunItem), 1);
	auto Press = [Abilities](const FGameplayTag& Input)
	{
		Abilities->AbilityInputTagPressed(Input);
		Abilities->AbilityInputTagReleased(Input);
	};
	auto AimAt = [Player](const AActor& Target)
	{
		FVector Origin;
		FVector Aim;
		UHollowHandsComponent::GetAimView(*Player, Origin, Aim);
		Player->GetController()->SetControlRotation((Target.GetActorLocation() - Origin).Rotation());
	};
	Press(HollowTags::Input_Hands_Weapon);
	HollowEnemyTests::Advance(Scene.TestWorld, 0.3f);
	int32 Shots = 0;
	for (; Shots < 10 && !Crate->IsBroken(); ++Shots)
	{
		AimAt(*Crate);
		Press(HollowTags::Input_PrimaryUse);
		HollowEnemyTests::Advance(Scene.TestWorld, 0.25f);
	}
	TestTrue(FString::Printf(TEXT("The nailgun breaks it (%d rounds)"), Shots), Crate->IsBroken() && Shots >= 2 && Shots <= 4);
	TestTrue(TEXT("...its loot spills"), !Crate->GetSpilled().IsEmpty());

	AHollowLootCrate* Second = SpawnCrate(Scene, DocksCrate, PlayerAt + FVector(110.0, 0.0, -70.0), Seed + 2);
	int32 Blows = 0;
	for (; Second && Blows < 10 && !Second->IsBroken(); ++Blows)
	{
		AimAt(*Second);
		Press(HollowTags::Input_Melee);
		HollowEnemyTests::Advance(Scene.TestWorld, 0.8f);
	}
	TestTrue(FString::Printf(TEXT("Blows break one too (%d)"), Blows), Second && Second->IsBroken() && Blows >= 2 && Blows <= 4);

	AimAt(*Scene.Door);
	const float DoorBefore = Scene.Door->GetDoorState()->GetStructure();
	for (int32 Shot = 0; Shot < 3; ++Shot)
	{
		AimAt(*Scene.Door);
		Press(HollowTags::Input_PrimaryUse);
		HollowEnemyTests::Advance(Scene.TestWorld, 0.25f);
	}
	TestEqual(TEXT("Shots do nothing to a door (only a breach)"), Scene.Door->GetDoorState()->GetStructure(), DoorBefore);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
