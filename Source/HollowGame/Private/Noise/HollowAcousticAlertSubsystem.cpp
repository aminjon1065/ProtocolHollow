#include "Noise/HollowAcousticAlertSubsystem.h"

#include "Engine/World.h"
#include "Framework/HollowPlayerController.h"
#include "GameFramework/Pawn.h"
#include "HollowGameLog.h"
#include "Noise/HollowNoiseSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowAcousticAlertSubsystem)

UHollowAcousticAlertSubsystem* UHollowAcousticAlertSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UHollowAcousticAlertSubsystem>() : nullptr;
}

bool UHollowAcousticAlertSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UHollowAcousticAlertSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UHollowNoiseSubsystem* Noise = Collection.InitializeDependency<UHollowNoiseSubsystem>())
	{
		AlertHandle = Noise->OnAcousticAlert.AddUObject(this, &ThisClass::HandleAlert);
	}
}

void UHollowAcousticAlertSubsystem::Deinitialize()
{
	if (UHollowNoiseSubsystem* Noise = UHollowNoiseSubsystem::Get(this))
	{
		Noise->OnAcousticAlert.Remove(AlertHandle);
	}
	Super::Deinitialize();
}

void UHollowAcousticAlertSubsystem::HandleAlert(const FHollowNoiseAnswer& Answer)
{
	const APawn* Player = Cast<APawn>(Answer.Instigator.Get());
	AHollowPlayerController* Controller = Player ? Cast<AHollowPlayerController>(Player->GetController()) : nullptr;
	if (!Controller)
	{
		return;
	}
	++AlertsSent;
	UE_LOG(LogHollowGame, Log, TEXT("Acoustic alert: %s answered %s's %s"), *GetNameSafe(Answer.Enemy.Get()), *Controller->GetName(),
		*Answer.NoiseTag.ToString());
	Controller->Client_AcousticAlert(Answer.NoiseTag);
}
