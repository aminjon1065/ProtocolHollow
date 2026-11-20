#include "Resonance/HollowSquadResonanceComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/HollowResonanceSet.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "HollowGASLog.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Resonance/HollowResonanceConfig.h"
#include "Resonance/HollowResonanceMath.h"
#include "Tags/HollowGameplayTags.h"
#include "TimerManager.h"
#include "Util/HollowEnumUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowSquadResonanceComponent)

UHollowSquadResonanceComponent::UHollowSquadResonanceComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

UHollowSquadResonanceComponent* UHollowSquadResonanceComponent::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->FindComponentByClass<UHollowSquadResonanceComponent>() : nullptr;
}

void UHollowSquadResonanceComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UHollowSquadResonanceComponent, State, Params);
}

float UHollowSquadResonanceComponent::GetNormalized() const
{
	return HollowResonanceMath::Dequantize(State.Level);
}

void UHollowSquadResonanceComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!Config)
	{
		Config = UHollowResonanceConfig::Get();
	}
	StartTimer();
}

void UHollowSquadResonanceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())   // null when the garbage collector ends play
	{
		World->GetTimerManager().ClearTimer(UpdateTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void UHollowSquadResonanceComponent::SetConfig(const UHollowResonanceConfig* InConfig)
{
	Config = InConfig;
	StartTimer();
}

void UHollowSquadResonanceComponent::StartTimer()
{
	UWorld* World = GetWorld();
	if (!World || !GetOwner() || !GetOwner()->HasAuthority() || !Config || !HasBegunPlay())
	{
		return;
	}
	World->GetTimerManager().SetTimer(UpdateTimer, this, &ThisClass::Evaluate, Config->UpdateSeconds, /*bLoop=*/true);
}

void UHollowSquadResonanceComponent::Evaluate()
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!Config || !GameState || !GetOwner()->HasAuthority())
	{
		return;
	}
	TArray<float> Living;
	for (const APlayerState* Player : GameState->PlayerArray)
	{
		const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Player);
		if (!AbilitySystem || !AbilitySystem->HasAttributeSetForAttribute(UHollowResonanceSet::GetResonanceAttribute())
			|| AbilitySystem->HasMatchingGameplayTag(HollowTags::State_Dead))
		{
			continue;
		}
		const float Max = AbilitySystem->GetNumericAttribute(UHollowResonanceSet::GetMaxResonanceAttribute());
		Living.Add(Max > 0.f ? AbilitySystem->GetNumericAttribute(UHollowResonanceSet::GetResonanceAttribute()) / Max : 0.f);
	}
	const float Squad = HollowResonanceMath::Aggregate(Living);
	FHollowSquadResonanceNetState Next;
	Next.Level = HollowResonanceMath::Quantize(Squad);
	Next.Tier = HollowResonanceMath::TierFor(Squad, State.Tier, Config->TierThresholds, Config->Hysteresis);
	if (Next == State)
	{
		return;
	}
	const EHollowResonanceTier OldTier = State.Tier;
	State = Next;
	MARK_PROPERTY_DIRTY_FROM_NAME(UHollowSquadResonanceComponent, State, this);
	UE_CLOG(OldTier != State.Tier, LogHollowGAS, Log, TEXT("Squad Resonance: %s -> %s"), *HollowEnumToString(OldTier), *Describe());
	OnSquadResonanceChanged.Broadcast(State);
}

void UHollowSquadResonanceComponent::OnRep_State(const FHollowSquadResonanceNetState& OldState)
{
	UE_CLOG(OldState.Tier != State.Tier, LogHollowGAS, Log, TEXT("Squad Resonance: %s -> %s (replicated)"), *HollowEnumToString(OldState.Tier), *Describe());
	OnSquadResonanceChanged.Broadcast(State);
}

FString UHollowSquadResonanceComponent::Describe() const
{
	return FString::Printf(TEXT("%s, level %d (%.3f)"), *HollowEnumToString(State.Tier), State.Level, GetNormalized());
}
