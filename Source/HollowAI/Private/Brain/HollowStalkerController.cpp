#include "Brain/HollowStalkerController.h"

#include "Abilities/HollowGA_EnemyAttack.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Enemies/HollowEnemyCharacter.h"
#include "Enemies/HollowStalkerDefinition.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "HollowAILog.h"
#include "Resonance/HollowResonanceMath.h"
#include "Resonance/HollowSquadResonanceComponent.h"
#include "Sector/HollowSectorTopologySubsystem.h"
#include "Threat/HollowActivityHeatmapSubsystem.h"
#include "Threat/HollowAIPlayers.h"
#include "Threat/HollowVentNetworkSubsystem.h"
#include "TimerManager.h"
#include "Util/HollowEnumUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowStalkerController)

AHollowStalkerController::AHollowStalkerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

const UHollowStalkerDefinition* AHollowStalkerController::GetStalkerDefinition() const
{
	return Cast<UHollowStalkerDefinition>(GetDefinition());
}

AHollowEnemyCharacter* AHollowStalkerController::GetEnemy() const
{
	return Cast<AHollowEnemyCharacter>(GetPawn());
}

bool AHollowStalkerController::IsHunting() const
{
	const UHollowThreatDirectorSubsystem* Director = UHollowThreatDirectorSubsystem::Get(this);
	return Director && Director->GetModeFor(GetPawn()) == EHollowThreatMode::Hunt;
}

bool AHollowStalkerController::IsRetreating() const
{
	const UHollowThreatDirectorSubsystem* Director = UHollowThreatDirectorSubsystem::Get(this);
	return Director && Director->GetModeFor(GetPawn()) == EHollowThreatMode::Retreat;
}

void AHollowStalkerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (UHollowThreatDirectorSubsystem* Director = UHollowThreatDirectorSubsystem::Get(this))
	{
		ModeHandle = Director->OnModeChanged.AddUObject(this, &ThisClass::HandleModeChanged);
	}
	if (UHollowSquadResonanceComponent* Found = UHollowSquadResonanceComponent::Get(this))
	{
		SquadResonance = Found;
		SquadHandle = Found->OnSquadResonanceChanged.AddUObject(this, &ThisClass::HandleSquadResonance);
		HandleSquadResonance(Found->GetState());
	}
}

void AHollowStalkerController::OnUnPossess()
{
	if (UHollowThreatDirectorSubsystem* Director = UHollowThreatDirectorSubsystem::Get(this))
	{
		Director->OnModeChanged.Remove(ModeHandle);
	}
	if (UHollowSquadResonanceComponent* Old = SquadResonance.Get())
	{
		Old->OnSquadResonanceChanged.Remove(SquadHandle);
	}
	SquadResonance.Reset();
	Super::OnUnPossess();
}

void AHollowStalkerController::HandleModeChanged(AHollowEnemyCharacter* Stalker, const EHollowThreatMode Mode)
{
	if (Stalker && Stalker == GetPawn())
	{
		Reconsider();
		if (Mode != EHollowThreatMode::Retreat && IsExposed())
		{
			GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::FleeTheLight);   // called back into the light: not inside the director's change
		}
	}
}

void AHollowStalkerController::FleeTheLight()
{
	UHollowThreatDirectorSubsystem* Director = UHollowThreatDirectorSubsystem::Get(this);
	if (Director && IsExposed())
	{
		Director->ReportRepelled(GetPawn());
	}
}

void AHollowStalkerController::HandleSquadResonance(const FHollowSquadResonanceNetState& State)
{
	const UHollowStalkerDefinition* Definition = GetStalkerDefinition();
	if (!Definition)
	{
		return;
	}
	const float Normalized = HollowResonanceMath::Dequantize(State.Level);
	const float Range = Definition->HearingRange * HollowResonanceMath::EvaluateMultiplier(Definition->HearingResonanceCurve, Normalized);
	if (!FMath::IsNearlyEqual(Range, GetHearingRange()))
	{
		UE_CLOG(State.Tier != HearingTier, LogHollowAI, Log, TEXT("%s: squad Resonance %.3f, hears %.0f cm"), *GetNameSafe(GetPawn()), Normalized, Range);
		UE_CLOG(State.Tier == HearingTier, LogHollowAI, Verbose, TEXT("%s: squad Resonance %.3f, hears %.0f cm"), *GetNameSafe(GetPawn()), Normalized, Range);
		HearingTier = State.Tier;
		SetHearingRange(Range);
	}
}

void AHollowStalkerController::OnExposed()
{
	FleeTheLight();
}

void AHollowStalkerController::OnStunned()
{
	if (UHollowThreatDirectorSubsystem* Director = UHollowThreatDirectorSubsystem::Get(this))
	{
		Director->ReportStunned(GetPawn());
	}
}

bool AHollowStalkerController::ShouldReconsiderOnPerception() const
{
	// Waiting in ambush, slipping away or striking it does not drop everything for a footstep: the ambush check and the
	// director decide. Shadowing or searching, it goes where it heard or saw the squad.
	return Activity == EHollowAIActivity::Track || Activity == EHollowAIActivity::Investigate || Activity == EHollowAIActivity::None;
}

void AHollowStalkerController::StartActivity(const EHollowAIActivity NewActivity)
{
	if (NewActivity != EHollowAIActivity::Retreat)
	{
		LeaveVent();   // called back by the director: out of the far vent first
	}
	switch (NewActivity)
	{
	case EHollowAIActivity::Track:   StartTrack(); break;
	case EHollowAIActivity::Ambush:  StartAmbush(); break;
	case EHollowAIActivity::Retreat: StartRetreat(); break;
	default:                         Super::StartActivity(NewActivity); break;
	}
}

void AHollowStalkerController::OnActivityEnded(const EHollowAIActivity Ended)
{
	GetWorldTimerManager().ClearTimer(AmbushCheckTimer);
	Phase = EPhase::None;   // a stalker in a vent stays there until its next activity is not a retreat
	QueryId = INDEX_NONE;
}

// ---------------------------------------------------------------------------------------------------- track and ambush

void AHollowStalkerController::StartTrack()
{
	const UHollowStalkerDefinition* Definition = GetStalkerDefinition();
	const UHollowActivityHeatmapSubsystem* Heatmap = UHollowActivityHeatmapSubsystem::Get(this);
	const UHollowSectorTopologySubsystem* Topology = GetTopology();
	const int32 Squad = Heatmap ? Heatmap->SquadRoom() : INDEX_NONE;
	if (!Definition || !Topology || Squad == INDEX_NONE)
	{
		FinishActivityAfter(true, Definition ? Definition->TrackPauseSeconds : UE_KINDA_SMALL_NUMBER);   // nobody to shadow yet
		return;
	}
	// The neighbour of the squad's room they have been in most: where they come from, or go back to.
	int32 Best = INDEX_NONE;
	for (const int32 Room : Topology->GetTopology().Neighbours(Squad))
	{
		if (Best == INDEX_NONE || Heatmap->GetRoomHeat(Room) > Heatmap->GetRoomHeat(Best))
		{
			Best = Room;
		}
	}
	if (Best == INDEX_NONE)
	{
		FinishActivityAfter(true, Definition->TrackPauseSeconds);
		return;
	}
	RunFocusQuery(Best);
}

void AHollowStalkerController::StartAmbush()
{
	const UHollowStalkerDefinition* Definition = GetStalkerDefinition();
	const UHollowActivityHeatmapSubsystem* Heatmap = UHollowActivityHeatmapSubsystem::Get(this);
	const int32 Room = Heatmap ? Heatmap->HottestRoom(/*bWithoutPlayers=*/true) : INDEX_NONE;
	if (!Definition || Room == INDEX_NONE)
	{
		FinishActivityAfter(false, Definition ? Definition->RetrySeconds : UE_KINDA_SMALL_NUMBER);   // no warm room to wait in
		return;
	}
	UE_LOG(LogHollowAI, Log, TEXT("%s: lies in wait in room %d (heat %.1f)"), *GetNameSafe(GetPawn()), Room, Heatmap->GetRoomHeat(Room));
	RunFocusQuery(Room);
}

void AHollowStalkerController::RunFocusQuery(const int32 Room)
{
	const UHollowStalkerDefinition* Definition = GetStalkerDefinition();
	FocusRoom = Room;
	Phase = EPhase::Searching;
	FEnvQueryRequest Request(Definition ? Definition->FocusQuery : nullptr, GetPawn());
	QueryId = Definition && Definition->FocusQuery ? Request.Execute(EEnvQueryRunMode::SingleResult, this, &ThisClass::HandleFocusQuery) : INDEX_NONE;
	if (QueryId == INDEX_NONE)
	{
		FinishActivityAfter(false, Definition ? Definition->RetrySeconds : UE_KINDA_SMALL_NUMBER);
	}
}

void AHollowStalkerController::HandleFocusQuery(TSharedPtr<FEnvQueryResult> Result)
{
	const UHollowStalkerDefinition* Definition = GetStalkerDefinition();
	const bool bCurrent = Result.IsValid() && Result->QueryID == QueryId && Phase == EPhase::Searching
		&& (Activity == EHollowAIActivity::Track || Activity == EHollowAIActivity::Ambush);
	if (!bCurrent || !Definition)
	{
		return;   // an answer for an activity that is over
	}
	if (!Result->IsSuccessful() || Result->Items.IsEmpty())
	{
		UE_LOG(LogHollowAI, Log, TEXT("%s: no dark spot in room %d"), *GetNameSafe(GetPawn()), FocusRoom);
		FinishActivityAfter(false, Definition->RetrySeconds);
		return;
	}
	const FVector Spot = Result->GetItemAsLocation(0);
	const float Speed = Activity == EHollowAIActivity::Ambush ? Definition->ChaseSpeed : Definition->PatrolSpeed;
	Phase = EPhase::Moving;
	if (!MoveForActivity(Spot, nullptr, Definition->MoveAcceptanceRadius, Speed))
	{
		FinishActivityAfter(false, Definition->RetrySeconds);
	}
}

void AHollowStalkerController::HandleActivityMove(const bool bSuccess)
{
	const UHollowStalkerDefinition* Definition = GetStalkerDefinition();
	if (!Definition)
	{
		return;
	}
	switch (Activity)
	{
	case EHollowAIActivity::Track:
		Phase = EPhase::Waiting;
		FinishActivityAfter(bSuccess, bSuccess ? Definition->TrackPauseSeconds : Definition->RetrySeconds);
		break;
	case EHollowAIActivity::Ambush:
		if (!bSuccess)
		{
			FinishActivityAfter(false, Definition->RetrySeconds);
			break;
		}
		Phase = EPhase::Waiting;
		GetWorldTimerManager().SetTimer(AmbushCheckTimer, this, &ThisClass::CheckAmbush, Definition->AmbushCheckSeconds, /*bLoop=*/true);
		FinishActivityAfter(true, Definition->AmbushWaitSeconds);   // nobody came: pick again
		break;
	case EHollowAIActivity::Retreat:
		if (bSuccess)
		{
			EnterVent();
		}
		else
		{
			FinishActivityAfter(false, Definition->RetrySeconds);
		}
		break;
	default:
		Super::HandleActivityMove(bSuccess);
		break;
	}
}

void AHollowStalkerController::CheckAmbush()
{
	const UHollowStalkerDefinition* Definition = GetStalkerDefinition();
	if (!Definition || Phase != EPhase::Waiting)
	{
		return;
	}
	APawn* Victim = nullptr;
	HollowAIPlayers::ForEach(GetWorld(), [&](APawn& Player)
	{
		if (!Victim && GapTo(&Player) <= Definition->AmbushTriggerRange && LineOfSightTo(&Player))
		{
			Victim = &Player;
		}
	});
	if (!Victim)
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(AmbushCheckTimer);
	ClearActivityTimer();
	Phase = EPhase::Striking;
	UE_LOG(LogHollowAI, Log, TEXT("%s: springs the ambush on %s"), *GetNameSafe(GetPawn()), *Victim->GetName());
	if (!TriggerAttack(Definition->MeleeEvent, Victim))
	{
		FinishActivityAfter(false, Definition->RetrySeconds);
	}
}

void AHollowStalkerController::HandleAbilityEnded(const FAbilityEndedData& Data)
{
	const UHollowGA_EnemyAttack* Attack = Cast<UHollowGA_EnemyAttack>(Data.AbilityThatEnded);
	const UHollowStalkerDefinition* Definition = GetStalkerDefinition();
	const bool bStrike = Attack && Definition && Attack->GetTriggerEvent() == Definition->MeleeEvent;
	if (bStrike && !Data.bWasCancelled)
	{
		if (UHollowThreatDirectorSubsystem* Director = UHollowThreatDirectorSubsystem::Get(this))
		{
			Director->ReportStrike(GetPawn());   // struck: back on the leash
		}
	}
	if (bStrike && Activity == EHollowAIActivity::Ambush && Phase == EPhase::Striking)
	{
		FinishActivityAfter(!Data.bWasCancelled, UE_KINDA_SMALL_NUMBER);
		return;
	}
	Super::HandleAbilityEnded(Data);
}

// ---------------------------------------------------------------------------------------------------- retreat

void AHollowStalkerController::StartRetreat()
{
	const UHollowStalkerDefinition* Definition = GetStalkerDefinition();
	if (const AHollowEnemyCharacter* Enemy = GetEnemy(); Enemy && Enemy->IsInVent())
	{
		Phase = EPhase::InVent;   // re-picked while hidden: keeps hiding, the director's next mode calls it out
		return;
	}
	const UHollowVentNetworkSubsystem* Vents = UHollowVentNetworkSubsystem::Get(this);
	const int32 Entry = Vents && GetPawn() ? Vents->NearestNode(GetPawn()->GetActorLocation()) : INDEX_NONE;
	if (!Definition || Entry == INDEX_NONE)
	{
		FinishActivityAfter(false, Definition ? Definition->RetrySeconds : UE_KINDA_SMALL_NUMBER);   // no vents in this sector
		return;
	}
	ExitVent = Entry;
	Phase = EPhase::ToVent;
	if (!MoveForActivity(Vents->GetNodeLocation(Entry), nullptr, Definition->MoveAcceptanceRadius, Definition->ChaseSpeed))
	{
		FinishActivityAfter(false, Definition->RetrySeconds);
	}
}

void AHollowStalkerController::EnterVent()
{
	AHollowEnemyCharacter* Enemy = GetEnemy();
	const UHollowVentNetworkSubsystem* Vents = UHollowVentNetworkSubsystem::Get(this);
	if (!Enemy || !Vents)
	{
		return;
	}
	const int32 Entry = ExitVent;
	TArray<FVector> Players;
	HollowAIPlayers::ForEach(GetWorld(), [&Players](APawn& Player) { Players.Add(Player.GetActorLocation()); });
	ExitVent = Vents->FarthestNodeFrom(Entry, Players);
	const float Trip = FMath::Max(Vents->TravelSeconds(Entry, ExitVent), 0.f);
	Enemy->SetInVent(true);
	Phase = EPhase::InVent;
	UE_LOG(LogHollowAI, Log, TEXT("%s: into vent %d, out of vent %d in %.1f s"), *Enemy->GetName(), Entry, ExitVent, Trip);
	GetWorldTimerManager().SetTimer(VentTripTimer, this, &ThisClass::FinishVentTrip, FMath::Max(Trip, UE_KINDA_SMALL_NUMBER), /*bLoop=*/false);
}

void AHollowStalkerController::FinishVentTrip()
{
	AHollowEnemyCharacter* Enemy = GetEnemy();
	const UHollowVentNetworkSubsystem* Vents = UHollowVentNetworkSubsystem::Get(this);
	if (!Enemy || !Vents || !Enemy->IsInVent())
	{
		return;
	}
	// At the far opening; it waits in the shaft until the director calls it back.
	const FVector Exit = Vents->GetNodeLocation(ExitVent) + FVector(0.0, 0.0, Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	Enemy->SetActorLocation(Exit, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
}

void AHollowStalkerController::LeaveVent()
{
	AHollowEnemyCharacter* Enemy = GetEnemy();
	if (!Enemy || !Enemy->IsInVent())
	{
		return;
	}
	if (GetWorldTimerManager().IsTimerActive(VentTripTimer))
	{
		GetWorldTimerManager().ClearTimer(VentTripTimer);
		FinishVentTrip();   // cut short: it comes out where it was going
	}
	Enemy->SetInVent(false);
	UE_LOG(LogHollowAI, Log, TEXT("%s: out of vent %d at %s"), *Enemy->GetName(), ExitVent, *Enemy->GetActorLocation().ToCompactString());
}

FString AHollowStalkerController::Describe() const
{
	const UHollowThreatDirectorSubsystem* Director = UHollowThreatDirectorSubsystem::Get(this);
	const AHollowEnemyCharacter* Enemy = GetEnemy();
	return FString::Printf(TEXT("%s, mode %s, focus room %d, hears %.0f cm%s"), *Super::Describe(),
		Director ? *HollowEnumToString(Director->GetModeFor(GetPawn())) : TEXT("-"), FocusRoom, GetHearingRange(), Enemy && Enemy->IsInVent() ? TEXT(", in a vent") : TEXT(""));
}
