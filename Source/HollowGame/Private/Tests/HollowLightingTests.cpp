#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/LightComponent.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialParameterCollection.h"
#include "Resonance/HollowResonancePresentationConfig.h"
#include "Rules/HollowLightBudget.h"
#include "Tags/HollowGameplayTags.h"
#include "Tests/HollowDocksTestRooms.h"

namespace HollowLightingTests
{
	using namespace HollowDocksTestRooms;

	// Content under test (create_lighting_assets.py, create_resonance_assets.py, create_docks_greybox.py).
	const TCHAR* const EmergencyFunction = TEXT("/Game/Hollow/World/Lighting/M_LF_Emergency.M_LF_Emergency");
	const TCHAR* const DyingFunction = TEXT("/Game/Hollow/World/Lighting/M_LF_DyingTube.M_LF_DyingTube");
	const TCHAR* const Emissive = TEXT("/Game/Hollow/World/Materials/M_Hollow_Emissive.M_Hollow_Emissive");
	const TCHAR* const EmergencyStrip = TEXT("/Game/Hollow/World/Materials/MI_Strip_Emergency.MI_Strip_Emergency");

	TArray<const ULightComponent*> LightsOf(const ULevel& Level)
	{
		TArray<const ULightComponent*> Lights;
		for (const AActor* Actor : Level.Actors)
		{
			TInlineComponentArray<ULightComponent*> Components(Actor);
			Lights.Append(Components);
		}
		return Lights;
	}

	/** Whether a material reads both Resonance scalars of the collection (the flicker in phase everywhere). */
	bool ReadsResonance(const UMaterial* Material, const UHollowResonancePresentationConfig& Presentation)
	{
		const UMaterialParameterCollection* Collection = Presentation.Collection.LoadSynchronous();
		bool bIntensity = false;
		bool bOffset = false;
#if WITH_EDITORONLY_DATA
		for (const TObjectPtr<UMaterialExpression>& Expression : Material ? Material->GetExpressions() : TConstArrayView<TObjectPtr<UMaterialExpression>>())
		{
			if (const UMaterialExpressionCollectionParameter* Parameter = Cast<UMaterialExpressionCollectionParameter>(Expression); Parameter && Parameter->Collection == Collection)
			{
				bIntensity |= Parameter->ParameterName == Presentation.IntensityParameter;
				bOffset |= Parameter->ParameterName == Presentation.TimeOffsetParameter;
			}
		}
#endif
		return bIntensity && bOffset;
	}

	/** A room level within its type's budget, with its emergency light (M_LF_Emergency, unshadowed), every light movable. */
	void CheckRoomLights(FAutomationTestBase& Test, const UHollowRoomDefinition& Room, const ULevel& Level, const UMaterialInterface* Emergency)
	{
		const TArray<FString> Problems = HollowLightBudget::CheckRoomLevel(Room, Level);
		Test.TestTrue(FString::Printf(TEXT("%s within budget%s%s"), *Room.GetName(), Problems.IsEmpty() ? TEXT("") : TEXT(": "), *FString::Join(Problems, TEXT("; "))),
			Problems.IsEmpty());
		const TArray<const ULightComponent*> Lights = LightsOf(Level);
		Test.TestTrue(FString::Printf(TEXT("%s has an emergency light"), *Room.GetName()), Lights.ContainsByPredicate([Emergency](const ULightComponent* Light)
		{
			return Light->LightFunctionMaterial == Emergency && !Light->CastShadows;
		}));
		Test.TestTrue(FString::Printf(TEXT("%s: every light movable (no baked light, ADR-0008)"), *Room.GetName()),
			!Lights.ContainsByPredicate([](const ULightComponent* Light) { return Light->Mobility != EComponentMobility::Movable; }));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowLightRoomBudgetsTest, "Hollow.Game.Light.RoomBudgets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowLightRoomBudgetsTest::RunTest(const FString& Parameters)
{
	// Every Docks room level within its type's budget (DA_LightBudget, ADR-0021), art-locked ones too (ADR-0027); every
	// room has its emergency light; the hero room's rig (HollowLightBudget::CheckHeroRig); the shipped limits keep a view
	// within 4 shadowed.
	using namespace HollowLightingTests;
	const UHollowLightBudgetConfig* Budget = UHollowLightBudgetConfig::Get();
	const UMaterialInterface* Emergency = LoadObject<UMaterialInterface>(nullptr, EmergencyFunction);
	const TArray<const UHollowRoomDefinition*> Rooms = DocksRoomDefinitions();
	if (!TestNotNull(TEXT("DA_LightBudget"), Budget) || !TestNotNull(TEXT("M_LF_Emergency"), Emergency) || !TestEqual(TEXT("Docks rooms"), Rooms.Num(), DocksRooms))
	{
		return false;
	}
	TestTrue(FString::Printf(TEXT("Any view holds at most %d shadowed lights"), Budget->MaxShadowedInView),
		HollowLightBudget::WorstShadowedInView(Budget->Rooms) <= Budget->MaxShadowedInView);
	for (const UHollowRoomDefinition* Room : Rooms)
	{
		const UWorld* Level = Room->RoomLevel.LoadSynchronous();
		if (!TestNotNull(*FString::Printf(TEXT("%s level"), *Room->GetName()), Level) || !Level->PersistentLevel)
		{
			continue;
		}
		CheckRoomLights(*this, *Room, *Level->PersistentLevel, Emergency);
		if (Room->GetName() == HeroRoom)
		{
			// The scripted rig: two shadowed panels, one failing; lit by hand (art-locked, ADR-0027): at least the failing one.
			const TArray<FString> Rig = HollowLightBudget::CheckHeroRig(HollowLightBudget::CollectLevel(*Level->PersistentLevel), Room->IsArtLocked());
			TestTrue(FString::Printf(TEXT("Hero room rig%s%s%s"), Room->IsArtLocked() ? TEXT(" (art-locked)") : TEXT(""), Rig.IsEmpty() ? TEXT("") : TEXT(": "),
				*FString::Join(Rig, TEXT("; "))), Rig.IsEmpty());
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowLightCathedralBudgetsTest, "Hollow.Game.Light.CathedralBudgets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowLightCathedralBudgetsTest::RunTest(const FString& Parameters)
{
	// S7-03 (ADR-0040): every Cathedral room level within its type's budget, with its emergency light flickering from
	// MPC_Resonance, every light movable; the two-storey halls are lit through their clerestory: a shadowed light that
	// casts a volumetric shadow (the shafts in the fog).
	using namespace HollowLightingTests;
	const UMaterialInterface* Emergency = LoadObject<UMaterialInterface>(nullptr, EmergencyFunction);
	const TArray<const UHollowRoomDefinition*> Rooms = SectorRoomDefinitions(CathedralRoot);
	if (!TestNotNull(TEXT("M_LF_Emergency"), Emergency) || !TestEqual(TEXT("Cathedral rooms"), Rooms.Num(), CathedralRooms))
	{
		return false;
	}
	for (const UHollowRoomDefinition* Room : Rooms)
	{
		const UWorld* Level = Room->RoomLevel.LoadSynchronous();
		if (!TestNotNull(*FString::Printf(TEXT("%s level"), *Room->GetName()), Level) || !Level->PersistentLevel)
		{
			continue;
		}
		CheckRoomLights(*this, *Room, *Level->PersistentLevel, Emergency);
		if (Room->Footprint.Z > 1 && !Room->RoomTags.HasTagExact(HollowTags::Room_Type_Junction))   // the crossing is a pass-through: no shadows
		{
			const TArray<const ULightComponent*> Lights = LightsOf(*Level->PersistentLevel);
			TestTrue(FString::Printf(TEXT("%s: lit through its clerestory, shafts in the fog"), *Room->GetName()),
				Lights.ContainsByPredicate([](const ULightComponent* Light) { return Light->CastShadows && Light->bCastVolumetricShadow; }));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowLightResonanceLinkTest, "Hollow.Game.Light.ResonanceLink",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowLightResonanceLinkTest::RunTest(const FString& Parameters)
{
	// The emergency lights, the failing panel and the emergency strips all flicker from MPC_Resonance (S4-02): its
	// intensity and the server-time offset, so the flicker is the squad's and in phase on every machine.
	using namespace HollowLightingTests;
	const UHollowResonancePresentationConfig* Presentation = UHollowResonancePresentationConfig::Get();
	const UMaterial* EmergencyLight = LoadObject<UMaterial>(nullptr, EmergencyFunction);
	const UMaterial* DyingLight = LoadObject<UMaterial>(nullptr, DyingFunction);
	const UMaterial* Strips = LoadObject<UMaterial>(nullptr, Emissive);
	const UMaterialInstanceConstant* Strip = LoadObject<UMaterialInstanceConstant>(nullptr, EmergencyStrip);
	if (!TestNotNull(TEXT("DA_ResonancePresentation"), Presentation) || !TestNotNull(TEXT("M_LF_Emergency"), EmergencyLight)
		|| !TestNotNull(TEXT("M_LF_DyingTube"), DyingLight) || !TestNotNull(TEXT("M_Hollow_Emissive"), Strips) || !TestNotNull(TEXT("MI_Strip_Emergency"), Strip))
	{
		return false;
	}
	TestTrue(TEXT("Both light functions are light functions"), EmergencyLight->MaterialDomain == MD_LightFunction && DyingLight->MaterialDomain == MD_LightFunction);
	TestTrue(TEXT("M_LF_Emergency reads the squad's Resonance and server time"), ReadsResonance(EmergencyLight, *Presentation));
	TestTrue(TEXT("M_LF_DyingTube too"), ReadsResonance(DyingLight, *Presentation));
	TestTrue(TEXT("M_Hollow_Emissive too"), ReadsResonance(Strips, *Presentation));
	float Follows = 0.f;
	TestTrue(TEXT("The emergency strips follow it"), Strip->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("FollowResonance")), Follows) && Follows == 1.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
