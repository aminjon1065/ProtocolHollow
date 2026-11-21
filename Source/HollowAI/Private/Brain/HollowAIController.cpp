#include "Brain/HollowAIController.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/HollowGA_EnemyAttack.h"
#include "Brain/HollowPathFollowingComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "Enemies/HollowEnemyCharacter.h"
#include "Enemies/HollowEnemyDefinition.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HollowAILog.h"
#include "Interfaces/HollowInteractable.h"
#include "Interfaces/HollowStructural.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "NavigationSystem.h"
#include "Noise/HollowNoiseConfig.h"
#include "Noise/HollowNoiseSubsystem.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"
#include "Sector/HollowSectorTopologySubsystem.h"
#include "StateTree.h"
#include "Tags/HollowGameplayTags.h"
#include "Threat/HollowAIPlayers.h"
#include "TimerManager.h"
#include "Util/HollowEnumUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowAIController)

AHollowAIController::AHollowAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UHollowPathFollowingComponent>(TEXT("PathFollowingComponent")))
{
	Brain = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("Brain"));
	Brain->SetStartLogicAutomatically(false);   // started on possession, with the definition's tree
	BrainComponent = Brain;

	Perception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	// Nobody has a team yet, and the default attitude solver calls equal teams friends: sense everyone and let
	// IsCandidate keep the living players.
	for (FAISenseAffiliationFilter* Filter : {&SightConfig->DetectionByAffiliation, &HearingConfig->DetectionByAffiliation})
	{
		Filter->bDetectEnemies = true;
		Filter->bDetectNeutrals = true;
		Filter->bDetectFriendlies = true;
	}
	Perception->ConfigureSense(*SightConfig);
	Perception->ConfigureSense(*HearingConfig);
	Perception->SetDominantSense(UAISense_Sight::StaticClass());
	SetPerceptionComponent(*Perception);
}

const UHollowEnemyDefinition* AHollowAIController::GetDefinition() const
{
	const AHollowEnemyCharacter* Enemy = Cast<AHollowEnemyCharacter>(GetPawn());
	return Enemy ? Enemy->GetDefinition() : nullptr;
}

void AHollowAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);   // the pawn grants its abilities here
	const UHollowEnemyDefinition* Definition = GetDefinition();
	if (!Definition || !Definition->StateTree)
	{
		UE_LOG(LogHollowAI, Error, TEXT("%s: %s has no enemy definition with a StateTree; it stays idle."), *GetName(), *GetNameSafe(InPawn));
		return;
	}
	Home = InPawn->GetActorLocation();
	DefaultNavigationFilterClass = Definition->NavigationFilter;
	ConfigureSenses(*Definition);
	Perception->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &ThisClass::HandlePerception);
	if (UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InPawn))
	{
		BoundAbilitySystem = AbilitySystem;
		AbilityEndedHandle = AbilitySystem->OnAbilityEnded.AddUObject(this, &ThisClass::HandleAbilityEnded);
		StunHandle = AbilitySystem->RegisterGameplayTagEvent(HollowTags::State_Stunned, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleStunChanged);
	}
	UE_LOG(LogHollowAI, Log, TEXT("%s: brain %s starts at %s"), *GetNameSafe(InPawn), *GetNameSafe(Definition->StateTree), *Home.ToCompactString());
	Brain->SetStateTree(Definition->StateTree);
	Brain->StartLogic();
}

void AHollowAIController::OnUnPossess()
{
	Shutdown();
	if (UAbilitySystemComponent* AbilitySystem = BoundAbilitySystem.Get())
	{
		AbilitySystem->OnAbilityEnded.Remove(AbilityEndedHandle);
		AbilitySystem->UnregisterGameplayTagEvent(StunHandle, HollowTags::State_Stunned, EGameplayTagEventType::NewOrRemoved);
	}
	BoundAbilitySystem.Reset();
	Perception->OnTargetPerceptionUpdated.RemoveDynamic(this, &ThisClass::HandlePerception);
	Super::OnUnPossess();
}

void AHollowAIController::Shutdown()
{
	if (Brain->IsRunning())
	{
		Brain->StopLogic(TEXT("Shutdown"));
	}
	EndActivity();
	StopMovement();
	GetWorldTimerManager().ClearTimer(ReconsiderTimer);
	GetWorldTimerManager().ClearTimer(ForgetTargetTimer);
}

void AHollowAIController::ConfigureSenses(const UHollowEnemyDefinition& Definition)
{
	SightConfig->SightRadius = Definition.SightRadius;
	SightConfig->LoseSightRadius = Definition.LoseSightRadius;
	SightConfig->PeripheralVisionAngleDegrees = Definition.SightHalfAngle;
	SightConfig->SetMaxAge(Definition.StimulusMaxAge);
	HearingConfig->HearingRange = Definition.HearingRange;
	HearingConfig->SetMaxAge(Definition.StimulusMaxAge);
	Perception->ConfigureSense(*SightConfig);
	Perception->ConfigureSense(*HearingConfig);
}

float AHollowAIController::GetHearingRange() const
{
	return HearingConfig->HearingRange;
}

void AHollowAIController::SetHearingRange(const float Range)
{
	if (FMath::IsNearlyEqual(HearingConfig->HearingRange, Range))
	{
		return;
	}
	HearingConfig->HearingRange = Range;
	Perception->ConfigureSense(*HearingConfig);
}

// ---------------------------------------------------------------------------------------------------- facts

double AHollowAIController::Now() const
{
	return GetWorld()->GetTimeSeconds();
}

const UHollowSectorTopologySubsystem* AHollowAIController::GetTopology() const
{
	return UHollowSectorTopologySubsystem::Get(this);
}

bool AHollowAIController::IsStunned() const
{
	const UAbilitySystemComponent* AbilitySystem = BoundAbilitySystem.Get();
	return AbilitySystem && AbilitySystem->HasMatchingGameplayTag(HollowTags::State_Stunned);
}

void AHollowAIController::HandleStunChanged(const FGameplayTag Tag, const int32 Count)
{
	if (Count > 0)
	{
		UE_LOG(LogHollowAI, Log, TEXT("%s: stunned"), *GetNameSafe(GetPawn()));
		// Whatever it was doing is over, not failed: the stopped move or the cancelled blow must not count against it
		// (a chase would mark its target unreachable). The brain picks Stunned on the reconsider below.
		EndActivity();
		if (UAbilitySystemComponent* AbilitySystem = BoundAbilitySystem.Get())
		{
			const FGameplayTagContainer Attacks(HollowTags::Ability_Enemy);
			AbilitySystem->CancelAbilities(&Attacks);   // a blow being wound up never lands
		}
		StopMovement();
		OnStunned();
		Reconsider();
	}
	else
	{
		UE_LOG(LogHollowAI, Log, TEXT("%s: recovered from the stun"), *GetNameSafe(GetPawn()));
		if (Activity == EHollowAIActivity::Stunned)
		{
			FinishActivityAfter(true, UE_KINDA_SMALL_NUMBER);
		}
	}
}

bool AHollowAIController::IsCandidate(const AActor* Actor) const
{
	return Actor && Actor != GetPawn() && HollowAIPlayers::IsLivingPlayer(Actor);
}

APawn* AHollowAIController::GetTarget() const
{
	if (Now() < TargetUnreachableUntil || !GetPawn())
	{
		return nullptr;
	}
	const UHollowEnemyDefinition* Definition = GetDefinition();
	if (APawn* Current = Target.Get(); Current && IsCandidate(Current)
		&& (Seen.Contains(Current) || (Definition && Now() - TargetLostTime <= Definition->TargetMemorySeconds)))
	{
		return Current;
	}
	APawn* Closest = nullptr;
	double ClosestDistance = TNumericLimits<double>::Max();
	for (const TWeakObjectPtr<APawn>& Weak : Seen)
	{
		APawn* Candidate = Weak.Get();
		const double Distance = Candidate ? FVector::DistSquared(Candidate->GetActorLocation(), GetPawn()->GetActorLocation()) : 0.0;
		if (Candidate && IsCandidate(Candidate) && Distance < ClosestDistance)
		{
			Closest = Candidate;
			ClosestDistance = Distance;
		}
	}
	Target = Closest;
	return Closest;
}

double AHollowAIController::GapTo(const AActor* Other) const
{
	const ACharacter* Self = Cast<ACharacter>(GetPawn());
	if (!Self || !Other)
	{
		return TNumericLimits<double>::Max();
	}
	return FVector::Dist2D(Self->GetActorLocation(), Other->GetActorLocation()) - Self->GetSimpleCollisionRadius() - Other->GetSimpleCollisionRadius();
}

bool AHollowAIController::IsTargetInEngageRange() const
{
	const UHollowEnemyDefinition* Definition = GetDefinition();
	const APawn* Current = GetTarget();
	return Definition && Current && GapTo(Current) <= Definition->EngageRange;
}

bool AHollowAIController::HasStimulus() const
{
	const UHollowEnemyDefinition* Definition = GetDefinition();
	return bHasStimulus && Definition && (Definition->StimulusMemorySeconds <= 0.f || Now() - StimulusTime <= Definition->StimulusMemorySeconds);
}

void AHollowAIController::ForgetStimulus()
{
	bHasStimulus = false;
}

void AHollowAIController::HandlePerception(AActor* Actor, const FAIStimulus Stimulus)
{
	APawn* SensedPawn = Cast<APawn>(Actor);
	if (!IsCandidate(SensedPawn))
	{
		return;
	}
	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>())
	{
		if (Stimulus.WasSuccessfullySensed())
		{
			ReportSeen(SensedPawn);
		}
		else
		{
			ReportLost(SensedPawn, Stimulus.StimulusLocation);
		}
	}
	else if (Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>() && Stimulus.WasSuccessfullySensed() && IsAudibleFromItsRoom(Stimulus))
	{
		ReportHeard(Stimulus.StimulusLocation);
	}
}

bool AHollowAIController::IsAudibleFromItsRoom(const FAIStimulus& Stimulus) const
{
	// Walls between rooms (ADR-0022): a noise from another room of the topology carries InterRoomRange of its range.
	const UHollowSectorTopologySubsystem* Topology = GetTopology();
	const UHollowNoiseSubsystem* Noise = UHollowNoiseSubsystem::Get(this);
	const UHollowNoiseConfig* Config = Noise ? Noise->GetConfig() : nullptr;
	const APawn* Listener = GetPawn();
	const FHollowNoiseRule* Rule = Config ? Config->FindRule(FGameplayTag::RequestGameplayTag(Stimulus.Tag, /*ErrorIfNotFound=*/false)) : nullptr;
	if (!Topology || !Rule || !Listener)
	{
		return true;
	}
	// Strength is the rule's loudness times the event's scale (footsteps by speed).
	const float Scale = Rule->Loudness > 0.f ? Stimulus.Strength / Rule->Loudness : 1.f;
	const bool bAudible = Config->ReachesThroughWalls(Topology->GetTopology(), *Rule, HearingConfig->HearingRange, Scale, Stimulus.StimulusLocation,
		Listener->GetActorLocation());
	UE_CLOG(!bAudible, LogHollowAI, Verbose, TEXT("%s: %s from another room, muffled by the walls"), *GetNameSafe(Listener), *Stimulus.Tag.ToString());
	return bAudible;
}

void AHollowAIController::ReportSeen(APawn* SeenPawn)
{
	if (!IsCandidate(SeenPawn))
	{
		return;
	}
	const APawn* Before = GetTarget();
	Seen.AddUnique(SeenPawn);
	if (SeenPawn == Target.Get())
	{
		GetWorldTimerManager().ClearTimer(ForgetTargetTimer);   // back in sight before it was forgotten
	}
	if (!Before && GetTarget())
	{
		UE_LOG(LogHollowAI, Log, TEXT("%s: sees %s"), *GetNameSafe(GetPawn()), *SeenPawn->GetName());
		if (ShouldReconsiderOnPerception())
		{
			Reconsider();
		}
	}
}

void AHollowAIController::ReportLost(APawn* LostPawn, const FVector& LastSeenLocation)
{
	const UHollowEnemyDefinition* Definition = GetDefinition();
	Seen.Remove(LostPawn);
	if (Target.Get() != LostPawn || !Definition)
	{
		return;
	}
	// Still hunted for a while; only then the brain is told and goes to where it was last seen.
	TargetLostTime = Now();
	TargetLastSeen = LastSeenLocation;
	GetWorldTimerManager().SetTimer(ForgetTargetTimer, this, &ThisClass::ForgetLostTarget, FMath::Max(Definition->TargetMemorySeconds, UE_KINDA_SMALL_NUMBER), /*bLoop=*/false);
}

void AHollowAIController::ForgetLostTarget()
{
	APawn* Lost = Target.Get();
	if (!Lost || Seen.Contains(Lost))
	{
		return;
	}
	Target.Reset();
	StimulusLocation = TargetLastSeen;
	StimulusTime = Now();
	bHasStimulus = true;
	UE_LOG(LogHollowAI, Log, TEXT("%s: lost %s, last seen at %s"), *GetNameSafe(GetPawn()), *Lost->GetName(), *TargetLastSeen.ToCompactString());
	if (ShouldReconsiderOnPerception())
	{
		Reconsider();
	}
}

void AHollowAIController::ReportHeard(const FVector& Location)
{
	const UHollowEnemyDefinition* Definition = GetDefinition();
	const bool bNew = !bHasStimulus || !Definition || FVector::Dist(Location, StimulusLocation) > Definition->MoveAcceptanceRadius;
	StimulusLocation = Location;
	StimulusTime = Now();
	bHasStimulus = true;
	// Continuous noise at the same spot (a welder) refreshes the memory; it does not restart the walk there or a breach
	// on the way, only wakes a brain that was patrolling.
	if (!GetTarget() && (bNew || Activity == EHollowAIActivity::Patrol || Activity == EHollowAIActivity::None) && ShouldReconsiderOnPerception())
	{
		UE_LOG(LogHollowAI, Log, TEXT("%s: hears a noise at %s"), *GetNameSafe(GetPawn()), *Location.ToCompactString());
		Reconsider();
	}
}

void AHollowAIController::ReportObstacle(AActor* InObstacle)
{
	if (!InObstacle || Activity == EHollowAIActivity::Breach)
	{
		return;
	}
	Obstacle = InObstacle;
	UE_LOG(LogHollowAI, Log, TEXT("%s: %s in the way"), *GetNameSafe(GetPawn()), *InObstacle->GetName());
	// Not inside path following's segment update: stop on the next tick, then let the brain decide.
	GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::StopAtObstacle);
}

void AHollowAIController::StopAtObstacle()
{
	if (!Obstacle.IsValid())
	{
		return;
	}
	EndActivity();   // the interrupted chase or patrol did not fail: it is just over
	StopMovement();
	Reconsider();
}

void AHollowAIController::MarkTargetUnreachable()
{
	const UHollowEnemyDefinition* Definition = GetDefinition();
	if (!Definition)
	{
		return;
	}
	UE_LOG(LogHollowAI, Log, TEXT("%s: cannot reach %s, ignores it for %.1f s"), *GetNameSafe(GetPawn()), *GetNameSafe(Target.Get()), Definition->RetrySeconds);
	Target.Reset();
	TargetUnreachableUntil = Now() + Definition->RetrySeconds;
	GetWorldTimerManager().SetTimer(ReconsiderTimer, this, &ThisClass::Reconsider, Definition->RetrySeconds, /*bLoop=*/false);
}

void AHollowAIController::Reconsider()
{
	if (Brain->IsRunning())
	{
		Brain->SendStateTreeEvent(HollowTags::AI_Event_Reconsider);
	}
}

// ---------------------------------------------------------------------------------------------------- activities

void AHollowAIController::BeginActivity(const EHollowAIActivity NewActivity, FOnHollowActivityFinished OnFinished)
{
	EndActivity();
	Activity = NewActivity;
	ActivityFinished = MoveTemp(OnFinished);
	UE_LOG(LogHollowAI, Log, TEXT("%s: %s (%s)"), *GetNameSafe(GetPawn()), *HollowEnumToString(NewActivity), *Describe());
	StartActivity(NewActivity);
}

void AHollowAIController::StartActivity(const EHollowAIActivity NewActivity)
{
	const UHollowEnemyDefinition* Definition = GetDefinition();
	switch (NewActivity)
	{
	case EHollowAIActivity::Patrol:      StartPatrol(); break;
	case EHollowAIActivity::Chase:       StartChase(); break;
	case EHollowAIActivity::Investigate: StartInvestigate(); break;
	case EHollowAIActivity::Attack:      StartAttack(); break;
	case EHollowAIActivity::Breach:      StartBreach(); break;
	case EHollowAIActivity::Stunned:
		StopMovement();
		if (!IsStunned())
		{
			FinishActivityAfter(true, UE_KINDA_SMALL_NUMBER);   // over already; otherwise the tag's removal ends it
		}
		break;
	case EHollowAIActivity::None:
		FinishActivityAfter(true, UE_KINDA_SMALL_NUMBER);
		break;
	default:
		UE_LOG(LogHollowAI, Warning, TEXT("%s: %s cannot %s"), *GetNameSafe(GetPawn()), *GetClass()->GetName(), *HollowEnumToString(NewActivity));
		FinishActivityAfter(false, Definition ? Definition->RetrySeconds : UE_KINDA_SMALL_NUMBER);
		break;
	}
}

void AHollowAIController::ClearActivityTimer()
{
	// Only a pending timer: one that is running right now is clearing itself, and clearing it would destroy its delegate mid-call.
	if (GetWorldTimerManager().IsTimerActive(ActivityTimer))
	{
		GetWorldTimerManager().ClearTimer(ActivityTimer);
	}
}

void AHollowAIController::EndActivity()
{
	if (Activity != EHollowAIActivity::None)
	{
		OnActivityEnded(Activity);
	}
	ClearActivityTimer();
	ActivityMove = FAIRequestID::InvalidRequest;
	Activity = EHollowAIActivity::None;
	ActivityFinished.Unbind();
	ClearFocus(EAIFocusPriority::Gameplay);
}

void AHollowAIController::FinishActivity(const bool bSuccess)
{
	if (Activity == EHollowAIActivity::None)
	{
		return;
	}
	const FOnHollowActivityFinished Finished = ActivityFinished;
	UE_LOG(LogHollowAI, Verbose, TEXT("%s: %s %s"), *GetNameSafe(GetPawn()), *HollowEnumToString(Activity), bSuccess ? TEXT("done") : TEXT("failed"));
	EndActivity();
	Finished.ExecuteIfBound(bSuccess);
}

void AHollowAIController::FinishActivityAfter(const bool bSuccess, const float Seconds)
{
	GetWorldTimerManager().SetTimer(ActivityTimer, FTimerDelegate::CreateUObject(this, &ThisClass::FinishActivity, bSuccess),
		FMath::Max(Seconds, UE_KINDA_SMALL_NUMBER), /*bLoop=*/false);
}

void AHollowAIController::FinishInvestigation(const bool bSuccess)
{
	ForgetStimulus();
	FinishActivity(bSuccess);
}

bool AHollowAIController::MoveForActivity(const FVector& Goal, AActor* GoalActor, const float AcceptanceRadius, const float Speed)
{
	const UHollowEnemyDefinition* Definition = GetDefinition();
	if (const ACharacter* Body = Cast<ACharacter>(GetPawn()))
	{
		Body->GetCharacterMovement()->MaxWalkSpeed = Speed;
	}
	ActivityMove = FAIRequestID::InvalidRequest;   // a request this one aborts is no longer ours
	FAIMoveRequest Request;
	if (GoalActor)
	{
		Request.SetGoalActor(GoalActor);
	}
	else
	{
		Request.SetGoalLocation(Goal);
	}
	Request.SetAcceptanceRadius(AcceptanceRadius);
	Request.SetNavigationFilter(Definition ? Definition->NavigationFilter : nullptr);
	Request.SetAllowPartialPath(true);
	Request.SetCanStrafe(false);
	const FPathFollowingRequestResult Result = MoveTo(Request);
	switch (Result.Code)
	{
	case EPathFollowingRequestResult::RequestSuccessful:
		ActivityMove = Result.MoveId;
		return true;
	case EPathFollowingRequestResult::AlreadyAtGoal:
		GetWorldTimerManager().SetTimer(ActivityTimer, FTimerDelegate::CreateUObject(this, &ThisClass::HandleActivityMove, true), UE_KINDA_SMALL_NUMBER, false);
		return true;
	default:
		return false;
	}
}

void AHollowAIController::OnMoveCompleted(const FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);
	if (ActivityMove.IsValid() && RequestID == ActivityMove)
	{
		ActivityMove = FAIRequestID::InvalidRequest;
		HandleActivityMove(Result.IsSuccess());
	}
}

void AHollowAIController::HandleActivityMove(const bool bSuccess)
{
	const UHollowEnemyDefinition* Definition = GetDefinition();
	if (!Definition)
	{
		return;
	}
	switch (Activity)
	{
	case EHollowAIActivity::Patrol:
		FinishActivityAfter(true, Definition->PatrolPauseSeconds);
		break;
	case EHollowAIActivity::Chase:
		if (bSuccess && IsTargetInEngageRange())
		{
			FinishActivity(true);
		}
		else
		{
			MarkTargetUnreachable();   // arrived (partial path) or gave up short of it
			FinishActivity(false);
		}
		break;
	case EHollowAIActivity::Investigate:
		if (bSuccess)
		{
			GetWorldTimerManager().SetTimer(ActivityTimer, FTimerDelegate::CreateUObject(this, &ThisClass::FinishInvestigation, true),
				Definition->InvestigateSeconds, /*bLoop=*/false);
		}
		else
		{
			ForgetStimulus();
			FinishActivityAfter(false, Definition->RetrySeconds);
		}
		break;
	default:
		break;
	}
}

bool AHollowAIController::FindPatrolPoint(FVector& OutPoint) const
{
	const UHollowEnemyDefinition* Definition = GetDefinition();
	const APawn* Self = GetPawn();
	const UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	ANavigationData* NavData = Navigation && Self ? Navigation->GetNavDataForProps(Self->GetNavAgentPropertiesRef(), Self->GetNavAgentLocation()) : nullptr;
	if (!Definition || !NavData)
	{
		return false;   // no navmesh yet (the sector builds it after spawning its actors)
	}
	FNavLocation Point;
	const FSharedConstNavQueryFilter Filter = UNavigationQueryFilter::GetQueryFilter(*NavData, this, Definition->NavigationFilter);
	if (!Navigation->GetRandomReachablePointInRadius(Home, Definition->PatrolRadius, Point, NavData, Filter))
	{
		return false;
	}
	OutPoint = Point.Location;
	return true;
}

void AHollowAIController::StartPatrol()
{
	const UHollowEnemyDefinition* Definition = GetDefinition();
	FVector Point;
	if (Definition && FindPatrolPoint(Point) && MoveForActivity(Point, nullptr, Definition->MoveAcceptanceRadius, Definition->PatrolSpeed))
	{
		return;
	}
	FinishActivityAfter(true, Definition ? Definition->PatrolPauseSeconds : UE_KINDA_SMALL_NUMBER);
}

void AHollowAIController::StartChase()
{
	const UHollowEnemyDefinition* Definition = GetDefinition();
	APawn* Current = GetTarget();
	if (!Definition || !Current)
	{
		FinishActivityAfter(false, Definition ? Definition->RetrySeconds : UE_KINDA_SMALL_NUMBER);
		return;
	}
	if (IsTargetInEngageRange())
	{
		FinishActivityAfter(true, UE_KINDA_SMALL_NUMBER);
		return;
	}
	if (!MoveForActivity(Current->GetActorLocation(), Current, Definition->ChaseStopDistance, Definition->ChaseSpeed))
	{
		MarkTargetUnreachable();
		FinishActivityAfter(false, UE_KINDA_SMALL_NUMBER);
	}
}

void AHollowAIController::StartInvestigate()
{
	const UHollowEnemyDefinition* Definition = GetDefinition();
	if (!Definition || !HasStimulus() || !MoveForActivity(StimulusLocation, nullptr, Definition->MoveAcceptanceRadius, Definition->ChaseSpeed))
	{
		ForgetStimulus();
		FinishActivityAfter(false, Definition ? Definition->RetrySeconds : UE_KINDA_SMALL_NUMBER);
	}
}

void AHollowAIController::StartAttack()
{
	const UHollowEnemyDefinition* Definition = GetDefinition();
	APawn* Current = GetTarget();
	if (!Definition || !Current || !TriggerAttack(Definition->MeleeEvent, Current))
	{
		FinishActivityAfter(false, Definition ? Definition->RetrySeconds : UE_KINDA_SMALL_NUMBER);
	}
}

void AHollowAIController::StartBreach()
{
	StopMovement();
	if (AActor* Current = Obstacle.Get())
	{
		SetFocus(Current, EAIFocusPriority::Gameplay);
	}
	GetWorldTimerManager().SetTimer(ActivityTimer, this, &ThisClass::StepBreach, UE_KINDA_SMALL_NUMBER, /*bLoop=*/false);
}

void AHollowAIController::StepBreach()
{
	const UHollowEnemyDefinition* Definition = GetDefinition();
	AActor* Current = Obstacle.Get();
	const IHollowStructural* Structural = Cast<IHollowStructural>(Current);
	if (!Definition || !Structural || Activity != EHollowAIActivity::Breach)
	{
		Obstacle.Reset();
		FinishActivity(true);
		return;
	}
	switch (Structural->GetPassage())
	{
	case EHollowPassage::Clear:
		UE_LOG(LogHollowAI, Log, TEXT("%s: %s no longer in the way"), *GetNameSafe(GetPawn()), *Current->GetName());
		Obstacle.Reset();
		FinishActivity(true);
		return;
	case EHollowPassage::Open:
		if (IHollowInteractable* Interactable = Cast<IHollowInteractable>(Current); Interactable && Interactable->CanInteract(GetPawn()))
		{
			UE_LOG(LogHollowAI, Log, TEXT("%s: opens %s"), *GetNameSafe(GetPawn()), *Current->GetName());
			Interactable->Interact(GetPawn());
		}
		GetWorldTimerManager().SetTimer(ActivityTimer, this, &ThisClass::StepBreach, Definition->ObstacleRecheckSeconds, /*bLoop=*/false);
		return;
	case EHollowPassage::Breach:
		if (!Definition->CanBreach())
		{
			// Welded or shut while it was on its way: its filter routes around such passages, so the next move does.
			UE_LOG(LogHollowAI, Log, TEXT("%s: cannot break %s; goes around"), *GetNameSafe(GetPawn()), *Current->GetName());
			Obstacle.Reset();
			FinishActivityAfter(false, Definition->RetrySeconds);
			return;
		}
		if (IsAttackActive(Definition->BreachEvent))
		{
			return;   // its end steps again
		}
		if (!TriggerAttack(Definition->BreachEvent, Current))
		{
			UE_LOG(LogHollowAI, Warning, TEXT("%s: cannot break %s; tries another way"), *GetNameSafe(GetPawn()), *Current->GetName());
			Obstacle.Reset();
			FinishActivityAfter(false, Definition->RetrySeconds);
		}
		return;
	}
}

bool AHollowAIController::TriggerAttack(const FGameplayTag& Event, AActor* AttackTarget)
{
	UAbilitySystemComponent* AbilitySystem = BoundAbilitySystem.Get();
	if (!AbilitySystem || !Event.IsValid())
	{
		return false;
	}
	FGameplayEventData Payload;
	Payload.EventTag = Event;
	Payload.Instigator = GetPawn();
	Payload.Target = AttackTarget;
	return AbilitySystem->HandleGameplayEvent(Event, &Payload) > 0;
}

bool AHollowAIController::IsAttackActive(const FGameplayTag& Event) const
{
	const UAbilitySystemComponent* AbilitySystem = BoundAbilitySystem.Get();
	if (!AbilitySystem)
	{
		return false;
	}
	for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
	{
		const UHollowGA_EnemyAttack* Attack = Cast<UHollowGA_EnemyAttack>(Spec.Ability);
		if (Attack && Spec.IsActive() && Attack->GetTriggerEvent() == Event)
		{
			return true;
		}
	}
	return false;
}

void AHollowAIController::HandleAbilityEnded(const FAbilityEndedData& Data)
{
	const UHollowGA_EnemyAttack* Attack = Cast<UHollowGA_EnemyAttack>(Data.AbilityThatEnded);
	const UHollowEnemyDefinition* Definition = GetDefinition();
	if (!Attack || !Definition)
	{
		return;
	}
	// Deferred: this runs inside EndAbility, too early to trigger the next blow.
	if (Activity == EHollowAIActivity::Attack && Attack->GetTriggerEvent() == Definition->MeleeEvent)
	{
		FinishActivityAfter(!Data.bWasCancelled, UE_KINDA_SMALL_NUMBER);
	}
	else if (Activity == EHollowAIActivity::Breach && Attack->GetTriggerEvent() == Definition->BreachEvent)
	{
		GetWorldTimerManager().SetTimer(ActivityTimer, this, &ThisClass::StepBreach, UE_KINDA_SMALL_NUMBER, /*bLoop=*/false);
	}
}

FString AHollowAIController::Describe() const
{
	const APawn* Self = GetPawn();
	const APawn* Current = GetTarget();
#if WITH_GAMEPLAY_DEBUGGER
	const FString States = FString::JoinBy(Brain->GetActiveStateNames(), TEXT(" > "), [](const FName& Name) { return Name.ToString(); });
#else
	const FString States = TEXT("n/a");   // the StateTree component lists its states only with the gameplay debugger (not in Shipping)
#endif
	return FString::Printf(TEXT("%s at %s, activity %s, states [%s], target %s, stimulus %s, obstacle %s"), *GetNameSafe(Self),
		Self ? *Self->GetActorLocation().ToCompactString() : TEXT("-"), *HollowEnumToString(Activity), *States,
		*GetNameSafe(Current), HasStimulus() ? *StimulusLocation.ToCompactString() : TEXT("none"), *GetNameSafe(Obstacle.Get()));
}
