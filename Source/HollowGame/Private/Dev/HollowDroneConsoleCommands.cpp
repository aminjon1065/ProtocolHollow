// Development console commands of the solo drone (DRONE-01, ADR-0054), for the solo smoke: look at the drone (to focus
// it for Interact), and dump its state. Never shipped.
#include "Carry/HollowCarrierComponent.h"
#include "Carry/HollowCarryableComponent.h"
#include "Character/HollowExoFrameCharacter.h"
#include "Drone/HollowSoloDrone.h"
#include "Drone/HollowSoloDroneSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HollowGameLog.h"
#include "Movement/HollowCharacterMovementComponent.h"
#include "Util/HollowEnumUtils.h"

#if !UE_BUILD_SHIPPING

namespace HollowDroneDev
{
	AHollowExoFrameCharacter* LocalPawn(UWorld* World)
	{
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		const APlayerController* Controller = GameInstance ? GameInstance->GetFirstLocalPlayerController(World) : nullptr;
		return Controller ? Cast<AHollowExoFrameCharacter>(Controller->GetPawn()) : nullptr;
	}

	/** The drone on this machine (a client has no subsystem record of it: the first one in the world). */
	AHollowSoloDrone* FindDrone(UWorld* World)
	{
		TActorIterator<AHollowSoloDrone> It(World);
		return It ? *It : nullptr;
	}

	/** Usage: Hollow.Dev.FaceDrone: the local player looks at its drone (focus it, then Hollow.Interact). */
	void FaceDrone(UWorld* World)
	{
		AHollowExoFrameCharacter* Pawn = LocalPawn(World);
		const AHollowSoloDrone* Drone = World ? FindDrone(World) : nullptr;
		AController* Controller = Pawn ? Pawn->GetController() : nullptr;
		if (!Drone || !Controller)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Hollow.Dev.FaceDrone: no local pawn or no drone."));
			return;
		}
		FVector Eyes;
		FRotator Unused;
		Pawn->GetActorEyesViewPoint(Eyes, Unused);
		const FVector Target = Drone->GetHover()->GetComponentLocation();
		Controller->SetControlRotation((Target - Eyes).Rotation());
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Dev.FaceDrone: %s faces %s, %.0f cm away"), *Pawn->GetName(), *Drone->GetName(), FVector::Dist(Eyes, Target));
	}

	/** Usage: Hollow.Drone.Dump: the drone's state on this machine, and its player's carry and pace. */
	void Dump(UWorld* World)
	{
		const AHollowSoloDrone* Drone = World ? FindDrone(World) : nullptr;
		const AHollowExoFrameCharacter* Pawn = LocalPawn(World);
		const UHollowCarrierComponent* Carrier = Pawn ? Pawn->GetCarrierComponent() : nullptr;
		const UHollowCarryableComponent* Carried = Carrier ? Carrier->GetCarried() : nullptr;
		const UHollowCharacterMovementComponent* Movement = Pawn ? Pawn->GetHollowMovement() : nullptr;
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Drone.Dump: %s; player %s carries %s (%s), max speed %.0f cm/s"),
			Drone ? *Drone->Describe() : TEXT("no drone"), *GetNameSafe(Pawn), Carried ? *GetNameSafe(Carried->GetOwner()) : TEXT("nothing"),
			Carried ? *HollowEnumToString(Carried->GetCarryState().Mode) : TEXT("None"), Movement ? Movement->GetMaxSpeed() : 0.f);
	}

	FAutoConsoleCommandWithWorld FaceDroneCommand(
		TEXT("Hollow.Dev.FaceDrone"),
		TEXT("The local player looks at its solo drone (focus it, then Hollow.Interact)."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&FaceDrone));

	FAutoConsoleCommandWithWorld DumpCommand(
		TEXT("Hollow.Drone.Dump"),
		TEXT("Logs the solo drone's state (player, light, hum, load, focus, distance) and its player's carry and max speed."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Dump));
}

#endif // !UE_BUILD_SHIPPING
