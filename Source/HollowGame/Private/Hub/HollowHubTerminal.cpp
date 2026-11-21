#include "Hub/HollowHubTerminal.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Framework/HollowPlayerController.h"
#include "GameFramework/Pawn.h"
#include "HollowGameLog.h"
#include "Util/HollowEnumUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowHubTerminal)

AHollowHubTerminal::AHollowHubTerminal(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;          // the interact request names it
	NetDormancy = DORM_Initial;  // level-placed, no state of its own to send
	Base = CreateDefaultSubobject<USceneComponent>(TEXT("Base"));
	SetRootComponent(Base);
	Cabinet = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cabinet"));
	Cabinet->SetupAttachment(Base);
	Screen = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Screen"));
	Screen->SetupAttachment(Base);
	Screen->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);   // the cabinet is what is focused
}

FVector AHollowHubTerminal::GetUseLocation() const
{
	return GetActorLocation() + GetActorForwardVector().GetSafeNormal2D() * UseDistance;
}

bool AHollowHubTerminal::IsHostSide(const AActor* Interactor)
{
	return Interactor && Interactor->GetNetMode() != NM_Client;
}

FHollowInteractionInfo AHollowHubTerminal::GetInteractionInfo(const AActor* Interactor) const
{
	FHollowInteractionInfo Info;
	Info.Prompt = !GuestPrompt.IsEmpty() && !IsHostSide(Interactor) ? GuestPrompt : Prompt;
	return Info;
}

bool AHollowHubTerminal::CanInteract(const AActor* Interactor) const
{
	const APawn* Pawn = Cast<APawn>(Interactor);
	return Pawn && Pawn->GetController() != nullptr;
}

void AHollowHubTerminal::Interact(AActor* Interactor)
{
	const APawn* Pawn = Cast<APawn>(Interactor);
	AHollowPlayerController* Player = Pawn ? Cast<AHollowPlayerController>(Pawn->GetController()) : nullptr;
	if (!HasAuthority() || !Player)
	{
		return;
	}
	UE_LOG(LogHollowGame, Log, TEXT("Hub: %s uses the %s terminal %s"), *Player->GetName(), *HollowEnumToString(Kind), *GetName());
	Player->Client_OpenTerminal(this);   // only that player's machine opens it
}
