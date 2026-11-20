#include "Raid/HollowRaidStatusWidget.h"

#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Foundation/HollowUISettings.h"
#include "Raid/HollowRaidStateComponent.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowRaidStatusWidget)

#define LOCTEXT_NAMESPACE "HollowRaidStatus"

namespace HollowRaidStatusPrivate
{
	/** Retry while the game state has not replicated yet. */
	constexpr float BindRetrySeconds = 0.5f;
}

UHollowRaidStatusWidget::UHollowRaidStatusWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::Collapsed);
}

void UHollowRaidStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();
	TryBind();
}

void UHollowRaidStatusWidget::NativeDestruct()
{
	if (UHollowRaidStateComponent* State = Raid.Get())
	{
		State->OnStateChanged.Remove(StateHandle);
	}
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimer);
		World->GetTimerManager().ClearTimer(BindTimer);
	}
	Super::NativeDestruct();
}

void UHollowRaidStatusWidget::TryBind()
{
	UWorld* World = GetWorld();
	UHollowRaidStateComponent* State = UHollowRaidStateComponent::Get(World);
	if (!State)
	{
		if (World)
		{
			World->GetTimerManager().SetTimer(BindTimer, this, &ThisClass::TryBind, HollowRaidStatusPrivate::BindRetrySeconds, /*bLoop=*/false);
		}
		return;
	}
	Raid = State;
	StateHandle = State->OnStateChanged.AddUObject(this, &ThisClass::HandleStateChanged);
	HandleStateChanged(State->GetState(), State->GetState());
}

void UHollowRaidStatusWidget::HandleStateChanged(const FHollowRaidNetState& OldState, const FHollowRaidNetState& NewState)
{
	FTimerManager& Timers = GetWorld()->GetTimerManager();
	if (NewState.PhaseEndServerTime > 0.f)
	{
		if (!Timers.IsTimerActive(RefreshTimer))
		{
			Timers.SetTimer(RefreshTimer, this, &ThisClass::Refresh, 1.f / FMath::Max(GetDefault<UHollowUISettings>()->RaidStatusRefreshRateHz, 1.f),
				/*bLoop=*/true);
		}
	}
	else
	{
		Timers.ClearTimer(RefreshTimer);
	}
	Refresh();
}

FText UHollowRaidStatusWidget::GetStatus() const
{
	const UHollowRaidStateComponent* State = Raid.Get();
	if (!State || !State->GetState().RaidId.IsValid())
	{
		return FText::GetEmpty();
	}
	const FHollowRaidNetState& Net = State->GetState();
	const int32 Seconds = FMath::CeilToInt(State->GetSecondsLeft());
	switch (Net.Phase)
	{
	case EHollowRaidPhase::Deploying:
		return FText::Format(LOCTEXT("Deploying", "DEPLOYING  {0}"), Seconds);
	case EHollowRaidPhase::Extraction:
		return FText::Format(LOCTEXT("Extraction", "SHUTTLE DEPARTS IN {0}  -  aboard {1} of {2}, cores {3}"), Seconds, Net.LivingInZone,
			Net.LivingPlayers, Net.CoresInZone);
	case EHollowRaidPhase::Debrief:
		return FText::Format(LOCTEXT("Debrief", "RAID OVER  -  back to the Skit in {0}"), Seconds);
	case EHollowRaidPhase::Assembling:
	case EHollowRaidPhase::InRaid:
		break;
	}
	return FText::GetEmpty();
}

void UHollowRaidStatusWidget::Refresh()
{
	const FText Status = GetStatus();
	StatusText->SetText(Status);
	SetVisibility(Status.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

#undef LOCTEXT_NAMESPACE
