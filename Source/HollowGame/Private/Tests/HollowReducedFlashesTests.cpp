#include "Tests/HollowGameTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Materials/Material.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialParameterCollection.h"
#include "Resonance/HollowResonancePresentationConfig.h"
#include "Resonance/HollowResonancePresentationSubsystem.h"
#include "Settings/HollowGameUserSettings.h"

namespace HollowReducedFlashesTests
{
	// Content under test (create_resonance_assets.py, create_lighting_assets.py): every material that flashes.
	const TCHAR* const FlashingMaterials[] = {
		TEXT("/Game/Hollow/FX/Resonance/M_PP_Resonance.M_PP_Resonance"),
		TEXT("/Game/Hollow/World/Lighting/M_LF_Emergency.M_LF_Emergency"),
		TEXT("/Game/Hollow/World/Lighting/M_LF_DyingTube.M_LF_DyingTube"),
		TEXT("/Game/Hollow/World/Materials/M_Hollow_Emissive.M_Hollow_Emissive"),
	};

	/** Whether Material reads Parameter of Collection (its graph: editor data). */
	bool ReadsCollectionScalar(const UMaterial& Material, const UMaterialParameterCollection* Collection, const FName Parameter)
	{
#if WITH_EDITORONLY_DATA
		for (const UMaterialExpression* Expression : Material.GetExpressions())
		{
			const UMaterialExpressionCollectionParameter* Read = Cast<UMaterialExpressionCollectionParameter>(Expression);
			if (Read && Read->Collection == Collection && Read->ParameterName == Parameter)
			{
				return true;
			}
		}
#endif
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowReducedFlashesTest, "Hollow.Game.Accessibility.ReducedFlashes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowReducedFlashesTest::RunTest(const FString& Parameters)
{
	// H-1 (ADR-0029): the player's reduced-flashes choice is a saved setting of this machine. The Resonance
	// presentation writes it into MPC_Resonance at once and on every change; every flashing material (post-process,
	// emergency and dying lights, emissive strips) reads it to hold steady or pulse slowly instead.
	using namespace HollowReducedFlashesTests;
	UHollowGameUserSettings* Settings = UHollowGameUserSettings::Get();
	if (!TestNotNull(TEXT("The game's settings class (GameUserSettingsClassName)"), Settings))
	{
		return false;
	}
	const bool bInitial = Settings->IsReducedFlashes();
	Settings->SetReducedFlashes(false, /*bSave=*/false);

	FTestWorldWrapper TestWorld;
	UWorld* World = HollowGameTests::CreateWorld(*this, TestWorld);
	UHollowResonancePresentationSubsystem* Presentation = UHollowResonancePresentationSubsystem::Get(World);
	const UHollowResonancePresentationConfig* Config = Presentation ? Presentation->GetConfig() : nullptr;
	const UMaterialParameterCollection* Collection = Config ? Config->Collection.LoadSynchronous() : nullptr;
	if (!World || !TestNotNull(TEXT("Presentation on this machine"), Presentation) || !TestNotNull(TEXT("DA_ResonancePresentation"), Config)
		|| !TestNotNull(TEXT("MPC_Resonance"), Collection))
	{
		Settings->SetReducedFlashes(bInitial, /*bSave=*/false);
		return false;
	}
	TestTrue(TEXT("The collection holds the scalar"), !Config->ReducedFlashesParameter.IsNone()
		&& Collection->GetScalarParameterByName(Config->ReducedFlashesParameter) != nullptr);
	TestEqual(TEXT("Full flashes by default: 0 in the collection"), Presentation->ReadCollectionScalar(Config->ReducedFlashesParameter), 0.f);

	int32 Changes = 0;
	const FDelegateHandle Handle = Settings->OnReducedFlashesChanged.AddLambda([&Changes]() { ++Changes; });
	Settings->SetReducedFlashes(true, /*bSave=*/false);
	TestTrue(TEXT("Reduced: announced, 1 in the collection at once"), Changes == 1 && UHollowResonancePresentationSubsystem::IsReducedFlashes()
		&& Presentation->ReadCollectionScalar(Config->ReducedFlashesParameter) == 1.f);
	Settings->SetReducedFlashes(true, /*bSave=*/false);
	TestEqual(TEXT("The same choice again announces nothing"), Changes, 1);
	Settings->SetReducedFlashes(false, /*bSave=*/false);
	TestTrue(TEXT("Back to full: 0 again"), Changes == 2 && Presentation->ReadCollectionScalar(Config->ReducedFlashesParameter) == 0.f);
	Settings->OnReducedFlashesChanged.Remove(Handle);

	for (const TCHAR* Path : FlashingMaterials)
	{
		const UMaterial* Material = LoadObject<UMaterial>(nullptr, Path);
		TestTrue(FString::Printf(TEXT("%s reads the reduced-flashes scalar"), Path),
			Material && ReadsCollectionScalar(*Material, Collection, Config->ReducedFlashesParameter));
	}
	Settings->SetReducedFlashes(bInitial, /*bSave=*/false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
