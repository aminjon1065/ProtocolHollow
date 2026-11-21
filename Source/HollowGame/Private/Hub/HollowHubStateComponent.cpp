#include "Hub/HollowHubStateComponent.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "HollowGameLog.h"
#include "Hub/HollowContractDefinition.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Sector/HollowSectorSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowHubStateComponent)

UHollowHubStateComponent::UHollowHubStateComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

UHollowHubStateComponent* UHollowHubStateComponent::Get(const UWorld* World)
{
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->FindComponentByClass<UHollowHubStateComponent>() : nullptr;
}

void UHollowHubStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UHollowHubStateComponent, SelectedContract, Params);
}

void UHollowHubStateComponent::BeginPlay()
{
	Super::BeginPlay();
	const FPrimaryAssetId& Default = GetDefault<UHollowSectorSettings>()->DefaultContract;
	if (GetOwner()->HasAuthority() && !SelectedContract.IsValid() && Default.IsValid())
	{
		SelectContract(Default);
	}
}

bool UHollowHubStateComponent::SelectContract(const FPrimaryAssetId& Contract)
{
	if (!GetOwner()->HasAuthority() || !UHollowContractDefinition::Exists(Contract))
	{
		UE_LOG(LogHollowGame, Warning, TEXT("Hub: contract %s refused (not on the server, or no such contract)"), *Contract.ToString());
		return false;
	}
	if (SelectedContract != Contract)
	{
		SelectedContract = Contract;
		MARK_PROPERTY_DIRTY_FROM_NAME(UHollowHubStateComponent, SelectedContract, this);
		UE_LOG(LogHollowGame, Log, TEXT("Hub: contract %s"), *Contract.ToString());
		OnContractChanged.Broadcast(SelectedContract);
	}
	return true;
}

void UHollowHubStateComponent::OnRep_SelectedContract()
{
	UE_LOG(LogHollowGame, Log, TEXT("Hub: the host chose contract %s"), *SelectedContract.ToString());
	OnContractChanged.Broadcast(SelectedContract);
}
