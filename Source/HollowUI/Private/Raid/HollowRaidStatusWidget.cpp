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
	constexpr int32 SecondsPerMinute = 60;
	constexpr int32 ClockSecondDigits = 2;

	/** m:ss, the visor's clock. */
	FText Clock(const int32 Seconds)
	{
		FNumberFormattingOptions TwoDigits;
		TwoDigits.MinimumIntegralDigits = ClockSecondDigits;
		const int32 Clamped = FMath::Max(Seconds, 0);
		return FText::Format(INVTEXT("{0}:{1}"), FText::AsNumber(Clamped / SecondsPerMinute), FText::AsNumber(Clamped % SecondsPerMinute, &TwoDigits));
	}
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
	const FText Left = HollowRaidStatusPrivate::Clock(FMath::CeilToInt(State->GetSecondsLeft()));
	switch (Net.Phase)
	{
	case EHollowRaidPhase::Deploying:
		return FText::Format(LOCTEXT("Deploying", "DEPLOY  {0}"), Left);
	case EHollowRaidPhase::Extraction:
		return FText::Format(LOCTEXT("Extraction", "EVAC  {0}   |   ABOARD {1}/{2}   |   CORES {3}"), Left, Net.LivingInZone, Net.LivingPlayers, Net.CoresInZone);
	case EHollowRaidPhase::Debrief:
		return FText::Format(LOCTEXT("Debrief", "RAID OVER   |   SKIT  {0}"), Left);
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
