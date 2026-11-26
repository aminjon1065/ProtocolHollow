#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/CameraActor.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/LightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/Level.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "FX/HollowFx.h"
#include "GameFramework/GameModeBase.h"
#include "Particles/ParticleSystem.h"   // UFXSystemAsset
#include "GameFramework/WorldSettings.h"
#include "Settings/HollowGameUserSettings.h"
#include "Tests/HollowGameTestUtils.h"
#include "World/HollowFxPlayerComponent.h"

namespace HollowMenuSceneTests
{
	const TCHAR* const BootMap = TEXT("/Game/Hollow/Maps/L_Boot.L_Boot");
	const FName SceneTag(TEXT("HollowMenuScene"));
	const TCHAR* const FrontEndMode = TEXT("BP_GameMode_FrontEnd");
	constexpr int32 MaxLights = 3;
	constexpr int32 MaxShadowed = 1;
	constexpr int32 MinProps = 12;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowMenuSceneTest, "Hollow.Game.MenuScene",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowMenuSceneTest::RunTest(const FString& Parameters)
{
	// UI-02 (ADR-0052): L_Boot is the live scene behind the main menu: the front end's game mode, a camera that player 0
	// views through, volumetric fog, at most three lights of its own (one shadowed), the Docks' art as looks only (no
	// collision, no navmesh) and the hovering core playing the lean FAB-01 aura, its dimmer variant under Reduced
	// Flashes. The aura follows that choice as soon as it changes (HollowFx::OnPlay: headless worlds spawn no Niagara).
	using namespace HollowMenuSceneTests;
	const UWorld* Boot = LoadObject<UWorld>(nullptr, BootMap);
	if (!TestNotNull(TEXT("L_Boot"), Boot) || !Boot->PersistentLevel)
	{
		return false;
	}
	const AWorldSettings* WorldSettings = Boot->PersistentLevel->GetWorldSettings();
	TestTrue(TEXT("The front end's game mode"), WorldSettings && WorldSettings->DefaultGameMode && WorldSettings->DefaultGameMode->GetName().Contains(FrontEndMode));
	int32 Lights = 0;
	int32 Shadowed = 0;
	int32 Props = 0;
	int32 Cameras = 0;
	const UHollowFxPlayerComponent* Aura = nullptr;
	bool bVolumetricFog = false;
	for (const AActor* Actor : Boot->PersistentLevel->Actors)
	{
		if (!Actor || !Actor->Tags.Contains(SceneTag))
		{
			continue;
		}
		for (const ULightComponent* Light : TInlineComponentArray<ULightComponent*>(Actor))
		{
			++Lights;
			Shadowed += Light->CastShadows ? 1 : 0;
		}
		if (const ACameraActor* Camera = Cast<ACameraActor>(Actor))
		{
			Cameras += Camera->GetAutoActivatePlayerIndex() == 0 ? 1 : 0;
		}
		if (const AExponentialHeightFog* Fog = Cast<AExponentialHeightFog>(Actor))
		{
			bVolumetricFog |= Fog->GetComponent() && Fog->GetComponent()->bEnableVolumetricFog;
		}
		if (const AStaticMeshActor* Mesh = Cast<AStaticMeshActor>(Actor))
		{
			const UStaticMeshComponent* Component = Mesh->GetStaticMeshComponent();
			++Props;
			TestTrue(FString::Printf(TEXT("%s: looks only"), *Actor->GetActorNameOrLabel()),
				Component && Component->GetCollisionEnabled() == ECollisionEnabled::NoCollision && !Component->CanEverAffectNavigation());
		}
		Aura = Aura ? Aura : Actor->FindComponentByClass<UHollowFxPlayerComponent>();
	}
	TestTrue(FString::Printf(TEXT("At most %d lights of its own (%d), %d shadowed (%d)"), MaxLights, Lights, MaxShadowed, Shadowed), Lights > 0 && Lights <= MaxLights && Shadowed <= MaxShadowed);
	TestEqual(TEXT("One camera for player 0"), Cameras, 1);
	TestTrue(TEXT("Volumetric fog"), bVolumetricFog);
	TestTrue(FString::Printf(TEXT("The hull and the Fab props (%d)"), Props), Props >= MinProps);
	if (!TestNotNull(TEXT("The core's aura"), Aura) || !TestTrue(TEXT("...with its Reduced Flashes variant"), Aura->GetEffect().IsSet() && !Aura->GetEffect().ReducedFlashesSystem.IsNull()))
	{
		return false;
	}

	// The aura's choice on this machine, now and when it changes.
	UHollowGameUserSettings* Settings = UHollowGameUserSettings::Get();
	FTestWorldWrapper TestWorld;
	UWorld* World = HollowGameTests::CreateWorld(*this, TestWorld);
	AActor* Holder = World ? World->SpawnActor<AActor>() : nullptr;
	if (!TestNotNull(TEXT("The engine's settings"), Settings) || !TestNotNull(TEXT("An actor"), Holder))
	{
		return false;
	}
	UHollowFxPlayerComponent* Player = NewObject<UHollowFxPlayerComponent>(Holder);
	Holder->SetRootComponent(Player);
	Player->RegisterComponent();
	const UFXSystemAsset* Full = Aura->GetEffect().System.LoadSynchronous();
	const UFXSystemAsset* Dim = Aura->GetEffect().ReducedFlashesSystem.LoadSynchronous();
	TArray<const UFXSystemAsset*> Played;
	const FDelegateHandle Handle = HollowFx::OnPlay().AddLambda([&Played](const FHollowFxSpec&, const UFXSystemAsset* System) { Played.Add(System); });
	const bool bKept = Settings->IsReducedFlashes();
	Settings->SetReducedFlashes(false, /*bSave=*/false);
	Holder->DispatchBeginPlay();
	Player->SetEffect(Aura->GetEffect());
	TestTrue(TEXT("Full flashes: the aura"), !Played.IsEmpty() && Played.Last() == Full);
	Settings->SetReducedFlashes(true, /*bSave=*/false);
	TestTrue(TEXT("Reduced flashes chosen: the dimmer aura at once"), !Played.IsEmpty() && Played.Last() == Dim);
	Settings->SetReducedFlashes(bKept, /*bSave=*/false);
	HollowFx::OnPlay().Remove(Handle);
	Holder->Destroy();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
