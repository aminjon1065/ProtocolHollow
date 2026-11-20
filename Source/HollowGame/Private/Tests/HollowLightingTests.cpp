#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/Count.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Components/LightComponent.h"
#include "Data/HollowRoomDefinition.h"
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

namespace HollowLightingTests
{
	// Content under test (create_lighting_assets.py, create_resonance_assets.py, create_docks_greybox.py).
	const TCHAR* const DocksRoot = TEXT("/HollowSector_Docks");
	const TCHAR* const HeroRoom = TEXT("DA_Room_Docks_CoreVault");
	const TCHAR* const EmergencyFunction = TEXT("/Game/Hollow/World/Lighting/M_LF_Emergency.M_LF_Emergency");
	const TCHAR* const DyingFunction = TEXT("/Game/Hollow/World/Lighting/M_LF_DyingTube.M_LF_DyingTube");
	const TCHAR* const Emissive = TEXT("/Game/Hollow/World/Materials/M_Hollow_Emissive.M_Hollow_Emissive");
	const TCHAR* const EmergencyStrip = TEXT("/Game/Hollow/World/Materials/MI_Strip_Emergency.MI_Strip_Emergency");
	constexpr int32 DocksRooms = 8;

	TArray<const UHollowRoomDefinition*> DocksRoomDefinitions()
	{
		IAssetRegistry& Registry = IAssetRegistry::GetChecked();
		Registry.ScanPathsSynchronous({FString(DocksRoot)}, /*bForceRescan=*/false);
		FARFilter Filter;
		Filter.PackagePaths.Add(FName(DocksRoot));
		Filter.bRecursivePaths = true;
		Filter.ClassPaths.Add(UHollowRoomDefinition::StaticClass()->GetClassPathName());
		TArray<FAssetData> Assets;
		Registry.GetAssets(Filter, Assets);
		Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.PackageName.LexicalLess(B.PackageName); });
		TArray<const UHollowRoomDefinition*> Rooms;
		for (const FAssetData& Asset : Assets)
		{
			if (const UHollowRoomDefinition* Room = Cast<UHollowRoomDefinition>(Asset.GetAsset()))
			{
				Rooms.Add(Room);
			}
		}
		return Rooms;
	}

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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowLightRoomBudgetsTest, "Hollow.Game.Light.RoomBudgets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowLightRoomBudgetsTest::RunTest(const FString& Parameters)
{
	// Every Docks room level within its type's budget (DA_LightBudget, ADR-0021); every room has its emergency light;
	// the hero room's rig: two shadowed panels (one of them failing), and the shipped limits keep a view within 4 shadowed.
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
		const TArray<FString> Problems = HollowLightBudget::CheckRoomLevel(*Room, *Level->PersistentLevel);
		TestTrue(FString::Printf(TEXT("%s within budget%s%s"), *Room->GetName(), Problems.IsEmpty() ? TEXT("") : TEXT(": "), *FString::Join(Problems, TEXT("; "))),
			Problems.IsEmpty());
		const TArray<const ULightComponent*> Lights = LightsOf(*Level->PersistentLevel);
		TestTrue(FString::Printf(TEXT("%s has an emergency light"), *Room->GetName()), Lights.ContainsByPredicate([Emergency](const ULightComponent* Light)
		{
			return Light->LightFunctionMaterial == Emergency && !Light->CastShadows;
		}));
		TestTrue(FString::Printf(TEXT("%s: every light movable (no baked light, ADR-0008)"), *Room->GetName()),
			!Lights.ContainsByPredicate([](const ULightComponent* Light) { return Light->Mobility != EComponentMobility::Movable; }));
		if (Room->GetName() == HeroRoom)
		{
			const TArray<FHollowLightInfo> Info = HollowLightBudget::CollectLevel(*Level->PersistentLevel);
			TestEqual(TEXT("Hero room: two shadowed panels"), static_cast<int32>(Algo::CountIf(Info, [](const FHollowLightInfo& L) { return L.bShadowed; })), 2);
			TestEqual(TEXT("...one of them failing (shadowed and flickering)"),
				static_cast<int32>(Algo::CountIf(Info, [](const FHollowLightInfo& L) { return L.bShadowed && L.bLightFunction; })), 1);
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
