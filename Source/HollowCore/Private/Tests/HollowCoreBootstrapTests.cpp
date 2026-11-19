#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Data/HollowAssetManager.h"
#include "Engine/Engine.h"
#include "GameplayTagsManager.h"
#include "Tags/HollowGameplayTags.h"
#include "Tags/HollowTagRules.h"

// Compile-time contract of the tag namespace rule (CLAUDE.md sec. 4).
static_assert(HollowTagRules::IsAllowedNamespace("Hollow.Ability.Tool.Welder"), "Hollow.* must be allowed");
static_assert(HollowTagRules::IsAllowedNamespace("GameplayCue.Hollow.Tool.Beam"), "GameplayCue.Hollow.* must be allowed");
static_assert(!HollowTagRules::IsAllowedNamespace("Ability.Tool.Welder"), "Tags outside Hollow.* must be rejected");
static_assert(!HollowTagRules::IsAllowedNamespace("GameplayCue.Tool.Beam"), "Cues outside GameplayCue.Hollow.* must be rejected");
static_assert(!HollowTagRules::IsAllowedNamespace("HollowExtra.Tag"), "The namespace match must include the separating dot");
static_assert(!HollowTagRules::IsAllowedNamespace("Hollow"), "A bare root is not a valid native tag");

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowCoreNativeTagsTest, "Hollow.Core.Tags.NativeHierarchy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowCoreNativeTagsTest::RunTest(const FString& Parameters)
{
	UGameplayTagsManager& TagsManager = UGameplayTagsManager::Get();
	const FGameplayTag HollowRoot = TagsManager.RequestGameplayTag(TEXT("Hollow"), /*ErrorIfNotFound=*/false);
	const FGameplayTag CueRoot = TagsManager.RequestGameplayTag(TEXT("GameplayCue"), /*ErrorIfNotFound=*/false);
	const FGameplayTag BeamCue = HollowTags::GameplayCue_Tool_Beam;

	TestTrue(TEXT("Implicit root 'Hollow' is registered"), HollowRoot.IsValid());
	TestTrue(TEXT("PlasmaCutter is a child of Hollow.Ability.Tool"),
		FGameplayTag(HollowTags::Ability_Tool_PlasmaCutter).MatchesTag(HollowTags::Ability_Tool));
	TestTrue(TEXT("Solo carry is a child of Hollow.State.CarryingHeavy"),
		FGameplayTag(HollowTags::State_CarryingHeavy_Solo).MatchesTag(HollowTags::State_CarryingHeavy));
	TestTrue(TEXT("Noise tags live under Hollow"), FGameplayTag(HollowTags::Noise_Voice).MatchesTag(HollowRoot));
	TestTrue(TEXT("Tool beam cue is discoverable under the GAS 'GameplayCue' root"), CueRoot.IsValid() && BeamCue.MatchesTag(CueRoot));
	TestFalse(TEXT("Cue tags never leak into the Hollow.* namespace"), BeamCue.MatchesTag(HollowRoot));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowCoreAssetManagerTest, "Hollow.Core.AssetManager.ConfiguredClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowCoreAssetManagerTest::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("GEngine"), GEngine))
	{
		return false;
	}

	TestTrue(TEXT("DefaultEngine.ini AssetManagerClassName points to UHollowAssetManager"),
		IsValid(Cast<UHollowAssetManager>(GEngine->AssetManager)));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
