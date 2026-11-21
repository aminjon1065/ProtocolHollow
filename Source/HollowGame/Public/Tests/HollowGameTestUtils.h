#pragma once

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Character/HollowExoFrameCharacter.h"
#include "Engine/World.h"
#include "Framework/HollowPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Voice/HollowVoiceComponent.h"
#include "Tests/AutomationCommon.h"

/** Shared fixtures of the HollowGame automation tests. */
namespace HollowGameTests
{
	/** Transient game world whose actors are initialized, so components behave as in a running game (no game mode, no content). */
	inline UWorld* CreateWorld(FAutomationTestBase& Test, FTestWorldWrapper& World)
	{
		if (!World.CreateTestWorld(EWorldType::Game))
		{
			World.ForwardErrorMessages(&Test);
			return nullptr;
		}
		World.GetTestWorld()->InitializeActorsForPlay(FURL());
		return World.GetTestWorld();
	}

	/** Spawns the bare C++ ExoFrame. Without a mesh (meshes are BP_ExoFrame data) its camera socket lookup warns. */
	inline AHollowExoFrameCharacter* SpawnExoFrame(FAutomationTestBase& Test, UWorld* World, const FTransform& Transform = FTransform::Identity)
	{
		Test.AddExpectedMessage(FString::Printf(TEXT("GetSocketInfoByName(%s): No SkeletalMesh"), *AHollowExoFrameCharacter::CameraSocketName.ToString()),
			ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, /*Occurrences=*/0, /*IsRegex=*/false);
		return World ? World->SpawnActor<AHollowExoFrameCharacter>(AHollowExoFrameCharacter::StaticClass(), Transform) : nullptr;
	}

	/**
	 * Possesses Character with a player controller owning an AHollowPlayerState, like a joined player on the server.
	 * bLocalPlayer: a standalone player on this machine (test worlds have no ULocalPlayer, so the engine would not
	 * call the controller local otherwise); needed for LocalPredicted abilities.
	 */
	template <typename TController = APlayerController>
	TController* PossessAsPlayer(UWorld* World, AHollowExoFrameCharacter* Character, const bool bLocalPlayer = false)
	{
		TController* Controller = World->SpawnActor<TController>();
		if (bLocalPlayer)
		{
			Controller->SetAsLocalPlayerController();
		}
		AHollowPlayerState* PlayerState = World->SpawnActor<AHollowPlayerState>();
		PlayerState->SetOwner(Controller);
		Controller->PlayerState = PlayerState;
		Controller->Possess(Character);
		return Controller;
	}

	/**
	 * Gives Controller the voice of AHollowPlayerController (S4-07), begun, so a test reports as the server would.
	 * Begins the controller's play too (test worlds do not begin play by themselves).
	 */
	inline UHollowVoiceComponent* AddVoice(AController* Controller)
	{
		if (!Controller)
		{
			return nullptr;
		}
		UHollowVoiceComponent* Voice = UHollowVoiceComponent::Get(Controller);
		if (!Voice)
		{
			Voice = NewObject<UHollowVoiceComponent>(Controller);
			Voice->RegisterComponent();
		}
		if (!Controller->HasActorBegunPlay())
		{
			Controller->DispatchBeginPlay();
		}
		return Voice;
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
