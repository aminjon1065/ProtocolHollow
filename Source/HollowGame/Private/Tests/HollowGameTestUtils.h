#pragma once

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Character/HollowExoFrameCharacter.h"
#include "Engine/World.h"
#include "Framework/HollowPlayerState.h"
#include "GameFramework/PlayerController.h"
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
	inline APlayerController* PossessAsPlayer(UWorld* World, AHollowExoFrameCharacter* Character, const bool bLocalPlayer = false)
	{
		APlayerController* Controller = World->SpawnActor<APlayerController>();
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
}

#endif // WITH_DEV_AUTOMATION_TESTS
