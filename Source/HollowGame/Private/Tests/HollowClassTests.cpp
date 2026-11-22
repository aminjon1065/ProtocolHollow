#include "Tests/HollowGameTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Attributes/HollowHealthSet.h"
#include "Attributes/HollowResonanceSet.h"
#include "Attributes/HollowToolSet.h"
#include "Classes/HollowClassComponent.h"
#include "Classes/HollowClassDefinition.h"
#include "Framework/HollowPlayerController.h"
#include "Framework/HollowPlayerState.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"
#include "Tags/HollowGameplayTags.h"

namespace HollowClassGameTests
{
	const TCHAR* const InitEffectClass = TEXT("/Game/Hollow/GAS/Effects/GE_ExoFrame_InitAttributes.GE_ExoFrame_InitAttributes_C");
	const FPrimaryAssetId Engineer(UHollowClassDefinition::AssetType, TEXT("DA_Class_Engineer"));
	const FPrimaryAssetId Occultist(UHollowClassDefinition::AssetType, TEXT("DA_Class_Occultist"));
	const FPrimaryAssetId NoClass(UHollowClassDefinition::AssetType, TEXT("Test_NoClass"));

	/** A joined player on the server with its starting attributes (the bare ExoFrame has no ability sets of its own). */
	struct FPlayer
	{
		AHollowPlayerController* Controller = nullptr;
		AHollowPlayerState* State = nullptr;
		UHollowAbilitySystemComponent* Abilities = nullptr;
		UHollowClassComponent* Class = nullptr;
	};

	bool Join(FAutomationTestBase& Test, UWorld* World, const TSubclassOf<UGameplayEffect>& InitClass, FPlayer& Out)
	{
		AHollowExoFrameCharacter* Body = HollowGameTests::SpawnExoFrame(Test, World);
		Out.Controller = Body ? HollowGameTests::PossessAsPlayer<AHollowPlayerController>(World, Body) : nullptr;
		Out.State = Out.Controller ? Out.Controller->GetPlayerState<AHollowPlayerState>() : nullptr;
		Out.Abilities = Out.State ? Out.State->GetHollowAbilitySystemComponent() : nullptr;
		Out.Class = Out.State ? Out.State->GetClassComponent() : nullptr;
		if (!Test.TestTrue(TEXT("A player with an ability system and a class component"), Out.Abilities && Out.Class))
		{
			return false;
		}
		Out.Abilities->ApplyGameplayEffectToSelf(InitClass.GetDefaultObject(), 1.f, Out.Abilities->MakeEffectContext());
		return true;
	}

	float Attribute(const FPlayer& Player, const FGameplayAttribute& Which)
	{
		return Player.Abilities->GetNumericAttribute(Which);
	}

	int32 EffectCount(const FPlayer& Player, const FPrimaryAssetId& Class)
	{
		const UHollowClassDefinition* Definition = UHollowClassDefinition::Load(Class);
		const TSubclassOf<UGameplayEffect> Effect = Definition ? Definition->AttributeEffect.LoadSynchronous() : nullptr;
		return Effect ? Player.Abilities->GetGameplayEffectCount(Effect, nullptr) : INDEX_NONE;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowClassSwitchTest, "Hollow.Game.Class.SwitchAndReplicate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowClassSwitchTest::RunTest(const FString& Parameters)
{
	// ADR-0034: the server gives each player its class's effect and tag (two players, two classes, different
	// attributes), a switch takes the old class's off, the class id replicates and a client hears it change.
	using namespace HollowClassGameTests;
	FTestWorldWrapper TestWorld;
	UWorld* World = HollowGameTests::CreateWorld(*this, TestWorld);
	const TSubclassOf<UGameplayEffect> InitClass = LoadClass<UGameplayEffect>(nullptr, InitEffectClass);
	FPlayer First;
	FPlayer Second;
	if (!World || !TestNotNull(TEXT("GE_ExoFrame_InitAttributes"), InitClass.Get()) || !Join(*this, World, InitClass, First) || !Join(*this, World, InitClass, Second))
	{
		return false;
	}
	const float Heat = Attribute(First, UHollowToolSet::GetMaxToolHeatAttribute());
	const float Armor = Attribute(First, UHollowHealthSet::GetArmorAttribute());
	const float Gain = Attribute(Second, UHollowResonanceSet::GetResonanceGainMultiplierAttribute());

	// Two players, two classes.
	TestTrue(TEXT("The first plays the Engineer"), First.Class->SetClass(Engineer) && First.Class->GetClassId() == Engineer);
	TestTrue(TEXT("The second plays the Techno-Occultist"), Second.Class->SetClass(Occultist) && Second.Class->GetClassId() == Occultist);
	TestTrue(TEXT("Engineer: its tag, hotter tools, more armour"), First.Abilities->HasMatchingGameplayTag(HollowTags::Class_Engineer)
		&& !First.Abilities->HasMatchingGameplayTag(HollowTags::Class_Occultist) && Attribute(First, UHollowToolSet::GetMaxToolHeatAttribute()) > Heat
		&& Attribute(First, UHollowHealthSet::GetArmorAttribute()) > Armor && EffectCount(First, Engineer) == 1);
	TestTrue(TEXT("Occultist: its tag, slower Resonance, the Engineer's things not"), Second.Abilities->HasMatchingGameplayTag(HollowTags::Class_Occultist)
		&& !Second.Abilities->HasMatchingGameplayTag(HollowTags::Class_Engineer) && Attribute(Second, UHollowResonanceSet::GetResonanceGainMultiplierAttribute()) < Gain
		&& Attribute(Second, UHollowToolSet::GetMaxToolHeatAttribute()) == Heat);
	TestTrue(TEXT("The class played again: nothing twice"), First.Class->SetClass(Engineer) && EffectCount(First, Engineer) == 1);

	// A switch takes the old class's grants off exactly.
	int32 Heard = 0;
	First.Class->OnClassChanged.AddLambda([&Heard](const FPrimaryAssetId&) { ++Heard; });
	TestTrue(TEXT("The first switches to the Occultist"), First.Class->SetClass(Occultist) && Heard == 1);
	TestTrue(TEXT("...the Engineer's tag and effect gone, its attributes back"), !First.Abilities->HasMatchingGameplayTag(HollowTags::Class_Engineer)
		&& EffectCount(First, Engineer) == 0 && Attribute(First, UHollowToolSet::GetMaxToolHeatAttribute()) == Heat
		&& Attribute(First, UHollowHealthSet::GetArmorAttribute()) == Armor);
	TestTrue(TEXT("...the Occultist's on"), First.Abilities->HasMatchingGameplayTag(HollowTags::Class_Occultist) && EffectCount(First, Occultist) == 1);

	// Refusals: not a class; the hub's request outside the hub (a test world is no hub).
	AddExpectedMessage(TEXT("refused (not on the server, or no such class)"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1, /*IsRegex=*/false);
	AddExpectedMessage(TEXT("only in the hub"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1, /*IsRegex=*/false);
	TestFalse(TEXT("Not a class: refused"), First.Class->SetClass(NoClass));
	Second.Controller->Server_SelectClass(Engineer);
	TestTrue(TEXT("...and a request outside the hub changes nothing"), First.Class->GetClassId() == Occultist && Second.Class->GetClassId() == Occultist);

	// Replication: the class id is replicated, push based, and a client's copy announces it.
	TestTrue(TEXT("The class component replicates"), First.Class->GetIsReplicated());
	TArray<FLifetimeProperty> Replicated;
	First.Class->GetLifetimeReplicatedProps(Replicated);
	const FProperty* ClassIdProperty = UHollowClassComponent::StaticClass()->FindPropertyByName(TEXT("ClassId"));
	TestTrue(TEXT("...its ClassId, push based, to everyone"), ClassIdProperty && Replicated.ContainsByPredicate([ClassIdProperty](const FLifetimeProperty& Each)
	{
		return Each.RepIndex == ClassIdProperty->RepIndex && Each.bIsPushBased && Each.Condition == COND_None;
	}));
	AHollowPlayerState* ClientCopy = World->SpawnActor<AHollowPlayerState>();
	UHollowClassComponent* ClientClass = ClientCopy ? ClientCopy->GetClassComponent() : nullptr;
	if (TestNotNull(TEXT("A copy as a client gets it"), ClientClass))
	{
		FPrimaryAssetId Announced;
		ClientClass->OnClassChanged.AddLambda([&Announced](const FPrimaryAssetId& Id) { Announced = Id; });
		ClientClass->ClassId = Engineer;   // what replication writes
		ClientClass->OnRep_ClassId();
		TestTrue(TEXT("...announced, with its definition (the visor's colour)"), Announced == Engineer && ClientClass->GetDefinition()
			&& ClientClass->GetDefinition()->ClassTag == HollowTags::Class_Engineer);
	}

	// Seamless travel: the next player state plays the same class.
	AHollowPlayerState* Travelled = World->SpawnActor<AHollowPlayerState>();
	if (TestNotNull(TEXT("A player state after travel"), Travelled))
	{
		Second.State->CopyProperties(Travelled);
		TestTrue(TEXT("...plays the same class, granted"), Travelled->GetClassComponent()->GetClassId() == Occultist
			&& Travelled->GetHollowAbilitySystemComponent()->HasMatchingGameplayTag(HollowTags::Class_Occultist));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
