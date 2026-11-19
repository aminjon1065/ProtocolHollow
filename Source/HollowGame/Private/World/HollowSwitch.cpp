#include "World/HollowSwitch.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "HollowGameLog.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowSwitch)

AHollowSwitch::AHollowSwitch(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	NetDormancy = DORM_Initial;   // level-placed: nothing to send until first used (ADR-0002)

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	Lamp = CreateDefaultSubobject<UPointLightComponent>(TEXT("Lamp"));
	Lamp->SetupAttachment(Mesh);
}

FHollowInteractionInfo AHollowSwitch::GetInteractionInfo(const AActor* Interactor) const
{
	FHollowInteractionInfo Info;
	Info.Prompt = bIsOn ? PromptTurnOff : PromptTurnOn;
	return Info;
}

bool AHollowSwitch::CanInteract(const AActor* Interactor) const
{
	return true;
}

void AHollowSwitch::Interact(AActor* Interactor)
{
	if (!HasAuthority())
	{
		return;
	}

	FlushNetDormancy();
	bIsOn = !bIsOn;
	MARK_PROPERTY_DIRTY_FROM_NAME(AHollowSwitch, bIsOn, this);
	ForceNetUpdate();

	UE_LOG(LogHollowGame, Log, TEXT("%s turned %s by %s"), *GetName(), bIsOn ? TEXT("on") : TEXT("off"), *GetNameSafe(Interactor));
	ApplyState();
}

void AHollowSwitch::BeginPlay()
{
	Super::BeginPlay();
	ApplyState();
}

void AHollowSwitch::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AHollowSwitch, bIsOn, Params);
}

void AHollowSwitch::OnRep_IsOn()
{
	UE_LOG(LogHollowGame, Log, TEXT("%s is %s (replicated)"), *GetName(), bIsOn ? TEXT("on") : TEXT("off"));
	ApplyState();
}

void AHollowSwitch::ApplyState()
{
	Lamp->SetVisibility(bIsOn);
	OnToggled.Broadcast(bIsOn);
}
