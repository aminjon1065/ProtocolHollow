#include "Voice/HollowVoiceTalker.h"

#include "Camera/CameraComponent.h"
#include "Character/HollowExoFrameCharacter.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "HollowGameLog.h"
#include "Voice/HollowVoiceConfig.h"
#include "Voice/HollowVoiceSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowVoiceTalker)

FOnHollowTalkingChanged UHollowVoiceTalker::OnTalkingChanged;

UHollowVoiceTalker::UHollowVoiceTalker(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHollowVoiceTalker::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;   // nobody listens there
	}
	if (APlayerState* State = GetOwner<APlayerState>())
	{
		State->OnPawnSet.AddUniqueDynamic(this, &ThisClass::HandlePawnSet);
	}
	TryRegister();
	RefreshPlacement();
}

void UHollowVoiceTalker::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APlayerState* State = GetOwner<APlayerState>())
	{
		State->OnPawnSet.RemoveDynamic(this, &ThisClass::HandlePawnSet);
	}
	if (bTalking)
	{
		bTalking = false;
		OnTalkingChanged.Broadcast(this, false);
	}
	Super::EndPlay(EndPlayReason);
}

void UHollowVoiceTalker::TryRegister()
{
	APlayerState* State = GetOwner<APlayerState>();
	if (bRegisteredForVoice || !State || !State->GetUniqueId().IsValid() || !HasBegunPlay() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	RegisterWithPlayerState(State);
	bRegisteredForVoice = true;
	UE_LOG(LogHollowGame, Verbose, TEXT("Voice: talker of %s registered"), *State->GetPlayerName());
}

void UHollowVoiceTalker::HandlePawnSet(APlayerState* Player, APawn* NewPawn, APawn* OldPawn)
{
	RefreshPlacement();
}

void UHollowVoiceTalker::RefreshPlacement()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const APlayerState* State = GetOwner<APlayerState>();
	const AHollowExoFrameCharacter* Body = State ? Cast<AHollowExoFrameCharacter>(State->GetPawn()) : nullptr;
	const UHollowVoiceConfig* Config = GetDefault<UHollowVoiceSettings>()->LoadConfig();
	const bool bAlive = Body && !Body->IsDead();
	Settings.ComponentToAttachTo = bAlive ? Body->GetFirstPersonCamera() : nullptr;   // the head of every copy of the body
	Settings.AttenuationSettings = bAlive && Config ? Config->Attenuation.Get() : nullptr;
	Settings.SourceEffectChain = Config ? (bAlive ? Config->HelmetChain.Get() : Config->RadioChain.Get()) : nullptr;
	bOnRadio = !bAlive;
}

void UHollowVoiceTalker::OnTalkingBegin(UAudioComponent* AudioComponent)
{
	Super::OnTalkingBegin(AudioComponent);
	bTalking = true;
	OnTalkingChanged.Broadcast(this, true);
}

void UHollowVoiceTalker::OnTalkingEnd()
{
	Super::OnTalkingEnd();
	bTalking = false;
	OnTalkingChanged.Broadcast(this, false);
}
