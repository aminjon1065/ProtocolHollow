#include "Engine/World.h"
#include "Framework/HollowPlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HollowGameLog.h"
#include "Voice/HollowVoiceComponent.h"
#include "Voice/HollowVoiceSubsystem.h"
#include "Voice/HollowVoiceTalker.h"

#if !UE_BUILD_SHIPPING

// Voice tools (S4-07, ADR-0025): a simulated microphone for tests and smokes, the mode, and the state of every talker.
namespace HollowVoiceConsolePrivate
{
	UHollowVoiceComponent* LocalVoice(const UWorld* World)
	{
		const APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
		return Player && Player->IsLocalController() ? UHollowVoiceComponent::Get(Player) : nullptr;
	}

	/** Usage: Hollow.Voice.Simulate <Level 0..255> <Seconds> */
	void Simulate(const TArray<FString>& Args, UWorld* World)
	{
		UHollowVoiceComponent* Voice = LocalVoice(World);
		const int32 Level = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : INDEX_NONE;
		if (!Voice || Level < 0 || Level > MAX_uint8 || !Args.IsValidIndex(1))
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Usage (with a local player): Hollow.Voice.Simulate <Level 0..255> <Seconds>"));
			return;
		}
		Voice->Simulate(static_cast<uint8>(Level), FCString::Atof(*Args[1]));
	}

	/** Usage: Hollow.Voice.Mode <PushToTalk|OpenMic|Muted> */
	void Mode(const TArray<FString>& Args, UWorld* World)
	{
		UHollowVoiceComponent* Voice = LocalVoice(World);
		const int64 Value = Args.IsEmpty() ? INDEX_NONE : StaticEnum<EHollowVoiceMode>()->GetValueByNameString(Args[0]);
		if (!Voice || Value == INDEX_NONE)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Usage (with a local player): Hollow.Voice.Mode <PushToTalk|OpenMic|Muted>"));
			return;
		}
		Voice->SetMode(static_cast<EHollowVoiceMode>(Value));
	}

	/** The local player's voice, the backend and every talker of this machine. */
	void Dump(UWorld* World)
	{
		const UHollowVoiceComponent* Voice = LocalVoice(World);
		const UHollowVoiceSubsystem* Backend = UHollowVoiceSubsystem::Get(World);
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Voice.Dump: %s; %s"), Voice ? *Voice->Describe() : TEXT("no local player"),
			Backend ? *Backend->Describe() : TEXT("no voice subsystem"));
		const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
		for (const APlayerState* State : GameState ? GameState->PlayerArray : TArray<TObjectPtr<APlayerState>>())
		{
			const AHollowPlayerState* HollowState = Cast<AHollowPlayerState>(State);
			const UHollowVoiceTalker* Talker = HollowState ? HollowState->GetVoiceTalker() : nullptr;
			if (Talker)
			{
				UE_LOG(LogHollowGame, Display, TEXT("Hollow.Voice.Dump: talker %s: registered %d, %s, talking %d"), *State->GetPlayerName(),
					Talker->IsRegisteredForVoice() ? 1 : 0, Talker->IsOnRadio() ? TEXT("radio") : TEXT("helmet"), Talker->IsTalking() ? 1 : 0);
			}
		}
	}

	FAutoConsoleCommandWithWorldAndArgs SimulateCommand(
		TEXT("Hollow.Voice.Simulate"),
		TEXT("Local player: for Seconds the microphone reads Level (0..255) and goes the real way to the server's AI. Usage: Hollow.Voice.Simulate <Level> <Seconds>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Simulate));

	FAutoConsoleCommandWithWorldAndArgs ModeCommand(
		TEXT("Hollow.Voice.Mode"),
		TEXT("Local player: how the voice goes out, kept in the settings. Usage: Hollow.Voice.Mode <PushToTalk|OpenMic|Muted>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Mode));

	FAutoConsoleCommandWithWorld DumpCommand(
		TEXT("Hollow.Voice.Dump"),
		TEXT("The local player's voice (mode, transmitting, level, server noises), the backend and every talker of this machine."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Dump));
}

#endif // !UE_BUILD_SHIPPING
