#include "Destruction/HollowDestructionBudgetSubsystem.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HollowPhysicsLog.h"
#include "Scalability.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowDestructionBudgetSubsystem)

bool UHollowDestructionBudgetSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UHollowDestructionBudgetSubsystem::Deinitialize()
{
	for (FLiveBreach& Breach : Live)
	{
		GetWorld()->GetTimerManager().ClearTimer(Breach.Timer);
	}
	Live.Reset();
	Super::Deinitialize();
}

FHollowDestructionBudget UHollowDestructionBudgetSubsystem::GetBudget() const
{
	return GetDefault<UHollowDestructionBudgetConfig>()->GetBudget(Scalability::GetQualityLevels().EffectsQuality);
}

TOptional<FVector> UHollowDestructionBudgetSubsystem::GetViewLocation() const
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Controller = It->Get();
		if (Controller && Controller->IsLocalController() && Controller->PlayerCameraManager)
		{
			return Controller->PlayerCameraManager->GetCameraLocation();
		}
	}
	return {};
}

EHollowBreachPresentation UHollowDestructionBudgetSubsystem::Choose(FHollowBreachPresentationInput Input) const
{
	Input.LiveBreaches = GetLiveBreaches();
	Input.LiveBodies = GetLiveBodies();
	return HollowBreachPolicy::Choose(Input, GetBudget());
}

int32 UHollowDestructionBudgetSubsystem::GetLiveBodies() const
{
	int32 Bodies = 0;
	for (const FLiveBreach& Breach : Live)
	{
		Bodies += Breach.Bodies;
	}
	return Bodies;
}

int32 UHollowDestructionBudgetSubsystem::BeginLive(const int32 Bodies, FSimpleDelegate OnExpired)
{
	const FHollowDestructionBudget Budget = GetBudget();
	if (Live.Num() >= Budget.MaxLiveBreaches || GetLiveBodies() + Bodies > Budget.MaxActiveBodies)
	{
		return INDEX_NONE;
	}
	FLiveBreach& Breach = Live.AddDefaulted_GetRef();
	Breach.Handle = NextHandle++;
	Breach.Bodies = Bodies;
	Breach.OnExpired = MoveTemp(OnExpired);
	GetWorld()->GetTimerManager().SetTimer(Breach.Timer, FTimerDelegate::CreateUObject(this, &ThisClass::Expire, Breach.Handle),
		FMath::Max(Budget.MaxDebrisLifetime, UE_KINDA_SMALL_NUMBER), /*bLoop=*/false);
	UE_LOG(LogHollowPhysics, Verbose, TEXT("Live fracture %d: %d bodies (%d live, %d bodies in all)"), Breach.Handle, Bodies, Live.Num(), GetLiveBodies());
	return Breach.Handle;
}

void UHollowDestructionBudgetSubsystem::EndLive(const int32 Handle)
{
	const int32 Index = Live.IndexOfByPredicate([Handle](const FLiveBreach& Breach) { return Breach.Handle == Handle; });
	if (Index != INDEX_NONE)
	{
		GetWorld()->GetTimerManager().ClearTimer(Live[Index].Timer);
		Live.RemoveAt(Index);
	}
}

void UHollowDestructionBudgetSubsystem::Expire(const int32 Handle)
{
	const int32 Index = Live.IndexOfByPredicate([Handle](const FLiveBreach& Breach) { return Breach.Handle == Handle; });
	if (Index == INDEX_NONE)
	{
		return;
	}
	const FSimpleDelegate OnExpired = Live[Index].OnExpired;
	Live.RemoveAt(Index);
	OnExpired.ExecuteIfBound();
}
