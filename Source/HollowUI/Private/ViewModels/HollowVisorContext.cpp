#include "ViewModels/HollowVisorContext.h"

#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Character/HollowExoFrameCharacter.h"
#include "CommonInputSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Foundation/HollowHUD.h"
#include "Foundation/HollowUISettings.h"
#include "GameFramework/PlayerController.h"
#include "HollowUILog.h"
#include "ViewModels/HollowPromptModel.h"
#include "ViewModels/HollowToolTargetModel.h"
#include "Voice/HollowVoiceComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowVisorContext)

void UHollowVisorContext::Initialize(APlayerController* InPlayer)
{
	Player = InPlayer;
	if (!ensure(InPlayer))
	{
		return;
	}
	InPlayer->OnPossessedPawnChanged.AddDynamic(this, &ThisClass::HandlePossessedPawnChanged);
	if (UGameInstance* GameInstance = InPlayer->GetGameInstance())
	{
		GameInstance->GetOnPawnControllerChanged().AddDynamic(this, &ThisClass::HandlePawnControllerChanged);
	}
	if (UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(InPlayer->GetLocalPlayer()))
	{
		InputTypeHandle = InputSubsystem->OnInputMethodChangedNative.AddWeakLambda(this, [this](const ECommonInputType InputType)
		{
			OnInputTypeChanged.Broadcast(InputType);
		});
	}

	const UHollowUISettings* Settings = GetDefault<UHollowUISettings>();
	Prompt = NewObject<UHollowPromptModel>(this);
	Prompt->Initialize(*this);
	ToolTarget = NewObject<UHollowToolTargetModel>(this);
	ToolTarget->Initialize(*this, Settings->TargetProbeRateHz, Settings->TargetLingerSeconds);
	SetCharacter(Cast<AHollowExoFrameCharacter>(InPlayer->GetPawn()));
}

void UHollowVisorContext::Deinitialize()
{
	SetCharacter(nullptr);
	if (Prompt)
	{
		Prompt->Deinitialize();
	}
	if (ToolTarget)
	{
		ToolTarget->Deinitialize();
	}
	if (APlayerController* PlayerController = Player.Get())
	{
		PlayerController->OnPossessedPawnChanged.RemoveAll(this);
		if (UGameInstance* GameInstance = PlayerController->GetGameInstance())
		{
			GameInstance->GetOnPawnControllerChanged().RemoveAll(this);
		}
		if (UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(PlayerController->GetLocalPlayer()))
		{
			InputSubsystem->OnInputMethodChangedNative.Remove(InputTypeHandle);
		}
	}
	Player.Reset();
}

UHollowVisorContext* UHollowVisorContext::Get(const APlayerController* InPlayer)
{
	const AHollowHUD* HUD = AHollowHUD::Get(InPlayer);
	return HUD ? HUD->GetVisorContext() : nullptr;
}

APlayerController* UHollowVisorContext::GetPlayer() const
{
	return Player.Get();
}

UHollowVoiceComponent* UHollowVisorContext::GetVoice() const
{
	return UHollowVoiceComponent::Get(Player.Get());
}

AHollowExoFrameCharacter* UHollowVisorContext::GetCharacter() const
{
	return Character.Get();
}

UHollowAbilitySystemComponent* UHollowVisorContext::GetAbilitySystem() const
{
	return AbilitySystem.Get();
}

ECommonInputType UHollowVisorContext::GetInputType() const
{
	const APlayerController* PlayerController = Player.Get();
	const UCommonInputSubsystem* InputSubsystem = PlayerController ? UCommonInputSubsystem::Get(PlayerController->GetLocalPlayer()) : nullptr;
	return InputSubsystem ? InputSubsystem->GetCurrentInputType() : ECommonInputType::MouseAndKeyboard;
}

UWorld* UHollowVisorContext::GetWorld() const
{
	const APlayerController* PlayerController = Player.Get();
	return PlayerController ? PlayerController->GetWorld() : nullptr;
}

void UHollowVisorContext::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	SetCharacter(Cast<AHollowExoFrameCharacter>(NewPawn));
}

void UHollowVisorContext::HandlePawnControllerChanged(APawn* Pawn, AController* Controller)
{
	if (Controller && Controller == Player.Get())
	{
		SetCharacter(Cast<AHollowExoFrameCharacter>(Pawn));
	}
	else if (Pawn && Pawn == Character.Get())
	{
		SetCharacter(nullptr);   // our pawn left us
	}
}

void UHollowVisorContext::SetCharacter(AHollowExoFrameCharacter* NewCharacter)
{
	if (Character.Get() == NewCharacter)
	{
		return;
	}
	if (AHollowExoFrameCharacter* OldCharacter = Character.Get())
	{
		OldCharacter->OnAbilitySystemChanged.Remove(AbilitySystemHandle);
	}
	Character = NewCharacter;
	AbilitySystem.Reset();
	if (NewCharacter)
	{
		AbilitySystemHandle = NewCharacter->OnAbilitySystemChanged.AddUObject(this, &ThisClass::HandleAbilitySystemChanged);
		AbilitySystem = NewCharacter->GetHollowAbilitySystemComponent();   // already the avatar on servers and hosts
	}
	UE_LOG(LogHollowUI, Log, TEXT("Visor of %s follows %s (ability system %s)"), *GetNameSafe(Player.Get()), *GetNameSafe(NewCharacter),
		AbilitySystem.IsValid() ? TEXT("ready") : TEXT("pending"));
	OnSourcesChanged.Broadcast();
}

void UHollowVisorContext::HandleAbilitySystemChanged(UHollowAbilitySystemComponent* NewAbilitySystem)
{
	if (AbilitySystem.Get() != NewAbilitySystem)
	{
		AbilitySystem = NewAbilitySystem;
		UE_LOG(LogHollowUI, Log, TEXT("Visor of %s: ability system %s"), *GetNameSafe(Player.Get()), NewAbilitySystem ? TEXT("ready") : TEXT("released"));
		OnSourcesChanged.Broadcast();
	}
}
