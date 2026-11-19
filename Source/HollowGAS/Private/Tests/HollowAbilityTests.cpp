#include "Tests/HollowGASTestRig.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/HollowAbilitySet.h"
#include "Abilities/HollowGameplayAbility.h"
#include "Misc/ScopeExit.h"
#include "Tags/HollowGameplayTags.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowAbilitySetGrantTest, "Hollow.GAS.AbilitySetGrant",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowAbilitySetGrantTest::RunTest(const FString& Parameters)
{
	FHollowGASTestRig Rig;
	if (!Rig.Create<>(*this))
	{
		return false;
	}

	UHollowAbilitySet* AbilitySet = NewObject<UHollowAbilitySet>();
	FHollowAbilitySetAbility& Entry = AbilitySet->GrantedAbilities.AddDefaulted_GetRef();
	Entry.Ability = UHollowGameplayAbility::StaticClass();
	Entry.InputTag = HollowTags::Input_PrimaryUse;

	FHollowAbilitySetHandles Handles;
	AbilitySet->GiveToAbilitySystem(Rig.AbilitySystem, &Handles);
	TestEqual(TEXT("The server grants the ability"), Rig.AbilitySystem->GetActivatableAbilities().Num(), 1);
	TestEqual(TEXT("The grant is tracked"), Handles.NumAbilities(), 1);
	if (Rig.AbilitySystem->GetActivatableAbilities().Num() == 1)
	{
		TestTrue(TEXT("The input tag travels with the spec"),
			Rig.AbilitySystem->GetActivatableAbilities()[0].GetDynamicSpecSourceTags().HasTagExact(HollowTags::Input_PrimaryUse));
	}

	Handles.TakeFromAbilitySystem(Rig.AbilitySystem);
	TestEqual(TEXT("Taking the set back removes the ability"), Rig.AbilitySystem->GetActivatableAbilities().Num(), 0);
	TestTrue(TEXT("Handles are cleared"), Handles.IsEmpty());

	// A client copy (no authority) must never grant (CLAUDE.md sec. 3).
	Rig.Owner->SetRole(ROLE_SimulatedProxy);
	Rig.AbilitySystem->SetIsReplicated(true);
	Rig.AbilitySystem->CacheIsNetSimulated();
	if (!TestFalse(TEXT("Precondition: the ability system now acts as a client copy"), Rig.AbilitySystem->IsOwnerActorAuthoritative()))
	{
		return false;
	}
	AbilitySet->GiveToAbilitySystem(Rig.AbilitySystem, &Handles);
	TestEqual(TEXT("Clients cannot grant ability sets"), Rig.AbilitySystem->GetActivatableAbilities().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowAbilityOnSpawnTest, "Hollow.GAS.AbilityOnSpawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowAbilityOnSpawnTest::RunTest(const FString& Parameters)
{
	FHollowGASTestRig Rig;
	if (!Rig.Create<>(*this))
	{
		return false;
	}

	int32 Activations = 0;
	Rig.AbilitySystem->AbilityActivatedCallbacks.AddLambda([&Activations](UGameplayAbility*) { ++Activations; });

	UHollowAbilitySet* AbilitySet = NewObject<UHollowAbilitySet>();
	AbilitySet->GrantedAbilities.AddDefaulted_GetRef().Ability = UHollowGameplayAbility::StaticClass();

	FHollowAbilitySetHandles InputHandles;
	AbilitySet->GiveToAbilitySystem(Rig.AbilitySystem, &InputHandles);
	TestEqual(TEXT("Input-driven abilities do not activate on grant"), Activations, 0);
	InputHandles.TakeFromAbilitySystem(Rig.AbilitySystem);

	// Switch the base class to OnSpawn for this test only.
	UHollowGameplayAbility* AbilityDefaults = GetMutableDefault<UHollowGameplayAbility>();
	const EHollowAbilityActivationPolicy SavedPolicy = AbilityDefaults->ActivationPolicy;
	AbilityDefaults->ActivationPolicy = EHollowAbilityActivationPolicy::OnSpawn;
	ON_SCOPE_EXIT { AbilityDefaults->ActivationPolicy = SavedPolicy; };

	FHollowAbilitySetHandles SpawnHandles;
	AbilitySet->GiveToAbilitySystem(Rig.AbilitySystem, &SpawnHandles);
	TestEqual(TEXT("OnSpawn abilities activate when granted"), Activations, 1);

	Rig.AbilitySystem->InitAbilityActorInfo(Rig.Owner, Rig.Owner);
	TestEqual(TEXT("Re-initializing the same avatar does not re-activate"), Activations, 1);

	Rig.AbilitySystem->CancelAbilities();
	AActor* NewAvatar = Rig.World.GetTestWorld()->SpawnActor<AActor>();
	Rig.AbilitySystem->InitAbilityActorInfo(Rig.Owner, NewAvatar);
	TestEqual(TEXT("A new avatar re-activates OnSpawn abilities"), Activations, 2);

	SpawnHandles.TakeFromAbilitySystem(Rig.AbilitySystem);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
