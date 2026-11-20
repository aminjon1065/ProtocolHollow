#include "Resonance/HollowResonancePresentationSubsystem.h"

#include "AudioModulationStatics.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "HollowGameLog.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Resonance/HollowResonanceMath.h"
#include "Resonance/HollowResonancePresentationConfig.h"
#include "Resonance/HollowSquadResonanceComponent.h"
#include "SoundControlBus.h"
#include "TimerManager.h"
#include "Util/HollowEnumUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowResonancePresentationSubsystem)

UHollowResonancePresentationSubsystem* UHollowResonancePresentationSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UHollowResonancePresentationSubsystem>() : nullptr;
}

bool UHollowResonancePresentationSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);   // nobody looks at a dedicated server
}

bool UHollowResonancePresentationSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UHollowResonancePresentationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SetConfig(UHollowResonancePresentationConfig::Get());
	if (UWorld* World = GetWorld())
	{
		GameStateHandle = World->GameStateSetEvent.AddUObject(this, &ThisClass::HandleGameStateSet);
	}
}

void UHollowResonancePresentationSubsystem::Deinitialize()
{
	Detach();
	if (UWorld* World = GetWorld())
	{
		World->GameStateSetEvent.Remove(GameStateHandle);
		World->GetTimerManager().ClearTimer(StepTimer);
	}
	Super::Deinitialize();
}

void UHollowResonancePresentationSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	HandleGameStateSet(InWorld.GetGameState());
}

void UHollowResonancePresentationSubsystem::HandleGameStateSet(AGameStateBase* GameState)
{
	if (UHollowSquadResonanceComponent* Found = GameState ? GameState->FindComponentByClass<UHollowSquadResonanceComponent>() : nullptr)
	{
		Attach(Found);
	}
}

void UHollowResonancePresentationSubsystem::SetConfig(const UHollowResonancePresentationConfig* InConfig)
{
	Config = InConfig;
	CollectionAsset = Config ? Config->Collection.LoadSynchronous() : nullptr;
	Bus = Config ? Config->Bus.LoadSynchronous() : nullptr;
}

void UHollowResonancePresentationSubsystem::Attach(UHollowSquadResonanceComponent* InSquad)
{
	if (!InSquad || InSquad == Squad.Get())
	{
		return;
	}
	Detach();
	Squad = InSquad;
	SquadHandle = InSquad->OnSquadResonanceChanged.AddUObject(this, &ThisClass::HandleSquadChanged);
	HandleSquadChanged(InSquad->GetState());   // whatever it is already (a client joining a raid in progress)
}

void UHollowResonancePresentationSubsystem::Detach()
{
	if (UHollowSquadResonanceComponent* Old = Squad.Get())
	{
		Old->OnSquadResonanceChanged.Remove(SquadHandle);
	}
	Squad.Reset();
}

void UHollowResonancePresentationSubsystem::HandleSquadChanged(const FHollowSquadResonanceNetState& State)
{
	UWorld* World = GetWorld();
	if (!World || !Config)
	{
		return;
	}
	const AGameStateBase* GameState = World->GetGameState();
	const bool bNewTier = State.Tier != Tier;
	Target = HollowResonanceMath::Dequantize(State.Level);
	Tier = State.Tier;
	TimeOffset = GameState ? static_cast<float>(GameState->GetServerWorldTimeSeconds() - World->GetTimeSeconds()) : 0.f;
	// A carried core moves the squad value every period: only a new tier is worth a line.
	UE_CLOG(bNewTier, LogHollowGame, Log, TEXT("Resonance presentation: %s, target %.3f, shown %.3f, time offset %.3f s"), *HollowEnumToString(Tier),
		Target, Intensity, TimeOffset);
	UE_CLOG(!bNewTier, LogHollowGame, Verbose, TEXT("Resonance presentation: target %.3f, shown %.3f"), Target, Intensity);
	Apply();   // the new time offset at once
	if (!FMath::IsNearlyEqual(Intensity, Target) && !World->GetTimerManager().IsTimerActive(StepTimer))
	{
		World->GetTimerManager().SetTimer(StepTimer, this, &ThisClass::Step, Config->StepSeconds, /*bLoop=*/true);
	}
}

void UHollowResonancePresentationSubsystem::Step()
{
	UWorld* World = GetWorld();
	if (!World || !Config)
	{
		return;
	}
	const float Delta = Config->StepSeconds / Config->EaseSeconds;
	Intensity = Intensity < Target ? FMath::Min(Intensity + Delta, Target) : FMath::Max(Intensity - Delta, Target);
	Apply();
	if (FMath::IsNearlyEqual(Intensity, Target))
	{
		Intensity = Target;
		World->GetTimerManager().ClearTimer(StepTimer);
		UE_LOG(LogHollowGame, Verbose, TEXT("Resonance presentation: %s"), *Describe());
	}
}

void UHollowResonancePresentationSubsystem::Apply()
{
	UWorld* World = GetWorld();
	if (!World || !Config)
	{
		return;
	}
	if (UMaterialParameterCollectionInstance* Instance = CollectionAsset ? World->GetParameterCollectionInstance(CollectionAsset) : nullptr)
	{
		Instance->SetScalarParameterValue(Config->IntensityParameter, Intensity);
		Instance->SetScalarParameterValue(Config->TimeOffsetParameter, TimeOffset);
	}
	if (Bus)
	{
		UAudioModulationStatics::SetGlobalBusMixValue(World, Bus, GetBusValue(), Config->BusFadeSeconds);
	}
}

float UHollowResonancePresentationSubsystem::GetBusValue() const
{
	return Config ? FMath::Lerp(Config->BusValueCalm, Config->BusValueFull, Intensity) : 0.f;
}

bool UHollowResonancePresentationSubsystem::IsEasing() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimerManager().IsTimerActive(StepTimer);
}

float UHollowResonancePresentationSubsystem::ReadCollectionScalar(const FName Parameter) const
{
	const UWorld* World = GetWorld();
	const UMaterialParameterCollectionInstance* Instance = World && CollectionAsset ? World->GetParameterCollectionInstance(CollectionAsset) : nullptr;
	float Value = 0.f;
	return Instance && Instance->GetScalarParameterValue(Parameter, Value) ? Value : 0.f;
}

FString UHollowResonancePresentationSubsystem::Describe() const
{
	// The flicker clock: Time + offset, what M_PP_Resonance flickers by; the same server time on every machine.
	const UWorld* World = GetWorld();
	const double FlickerClock = (World ? World->GetTimeSeconds() : 0.0) + TimeOffset;
	return FString::Printf(TEXT("shown %.3f of %.3f (%s), MPC intensity %.3f, time offset %.3f s, flicker clock %.2f s, bus %.3f"), Intensity, Target,
		*HollowEnumToString(Tier), Config ? ReadCollectionScalar(Config->IntensityParameter) : 0.f, TimeOffset, FlickerClock, GetBusValue());
}
