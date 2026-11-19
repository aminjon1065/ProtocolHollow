// Development console commands for headless and multi-process testing. Never shipped.
#include "Character/HollowExoFrameCharacter.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HollowGameLog.h"
#include "Interaction/HollowInteractionComponent.h"
#include "TimerManager.h"

#if !UE_BUILD_SHIPPING

namespace HollowDevConsole
{
	/** Same as pressing Interact: asks the server to interact with the local player's focus. */
	void Interact(UWorld* World)
	{
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		const APlayerController* Controller = GameInstance ? GameInstance->GetFirstLocalPlayerController(World) : nullptr;
		const AHollowExoFrameCharacter* Character = Controller ? Cast<AHollowExoFrameCharacter>(Controller->GetPawn()) : nullptr;
		if (!Character)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Hollow.Interact: no local ExoFrame."));
			return;
		}
		FVector EyeLocation;
		FRotator EyeRotation;
		Character->GetActorEyesViewPoint(EyeLocation, EyeRotation);
		UE_LOG(LogHollowGame, Log, TEXT("Hollow.Interact: %s at %s, eyes %s looking %s, requests %s"), *Character->GetName(),
			*Character->GetActorLocation().ToCompactString(), *EyeLocation.ToCompactString(), *EyeRotation.ToCompactString(),
			*GetNameSafe(Character->GetInteractionComponent()->GetFocusedActor()));
		Character->GetInteractionComponent()->RequestInteract();
	}

	/** Runs a console command later; the timer lives on the game instance, so it survives map travel. */
	void RunAfter(const TArray<FString>& Args, UWorld* World)
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		if (Args.Num() < 2 || !GameInstance)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Usage: Hollow.Dev.RunAfter <seconds> <command...>"));
			return;
		}

		const float DelaySeconds = FCString::Atof(*Args[0]);
		const FString Command = FString::Join(TArrayView<const FString>(Args).RightChop(1), TEXT(" "));
		FTimerHandle Handle;
		GameInstance->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(GameInstance, [GameInstance, Command]()
		{
			UE_LOG(LogHollowGame, Log, TEXT("Hollow.Dev.RunAfter: %s"), *Command);
			GEngine->Exec(GameInstance->GetWorld(), *Command);
		}), FMath::Max(DelaySeconds, UE_KINDA_SMALL_NUMBER), /*bLoop=*/false);
	}

	FAutoConsoleCommandWithWorld InteractCommand(
		TEXT("Hollow.Interact"),
		TEXT("Interacts with the local player's current focus (same as the Interact input)."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Interact));

	FAutoConsoleCommandWithWorldAndArgs RunAfterCommand(
		TEXT("Hollow.Dev.RunAfter"),
		TEXT("Runs a console command after a delay that survives map travel. Usage: Hollow.Dev.RunAfter <seconds> <command...>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunAfter));
}

#endif // !UE_BUILD_SHIPPING
