#pragma once

#include "AIController.h"
#include "Brain/HollowAITypes.h"
#include "GameplayTagContainer.h"
#include "Perception/AIPerceptionTypes.h"

#include "HollowAIController.generated.h"

class UAbilitySystemComponent;
class UHollowSectorTopologySubsystem;
class UAISenseConfig_Hearing;
class UAISenseConfig_Sight;
class UHollowEnemyDefinition;
class UStateTreeAIComponent;
struct FAbilityEndedData;

DECLARE_DELEGATE_OneParam(FOnHollowActivityFinished, bool /*bSuccess*/);

/**
 * Server-side brain body of an enemy (Docs/Architecture/02 sec. 2.7, ADR-0017). Keeps the facts the StateTree decides
 * on (target in sight, last stimulus, obstacle on the path) and runs the activity the tree picked: movement with the
 * definition's query filter, timers, and attack abilities triggered by gameplay events. Event-driven throughout:
 * perception updates, move and ability completions; facts that change the picture send Hollow.AI.Event.Reconsider.
 */
UCLASS()
class HOLLOWAI_API AHollowAIController : public AAIController
{
	GENERATED_BODY()

public:
	explicit AHollowAIController(const FObjectInitializer& ObjectInitializer);

	const UHollowEnemyDefinition* GetDefinition() const;
	UStateTreeAIComponent* GetBrain() const { return Brain; }

	/**
	 * The player it hunts: alive, not marked out of reach, in sight or lost from sight less than TargetMemorySeconds ago;
	 * the closest seen one if that one is gone.
	 */
	APawn* GetTarget() const;
	bool IsTargetInEngageRange() const;
	/** A stimulus recent enough to investigate. */
	bool HasStimulus() const;
	const FVector& GetStimulusLocation() const { return StimulusLocation; }
	AActor* GetObstacle() const { return Obstacle.Get(); }
	EHollowAIActivity GetActivity() const { return Activity; }

	// Facts: perception reports them (tests may report directly).
	void ReportSeen(APawn* SeenPawn);
	void ReportLost(APawn* LostPawn, const FVector& LastSeenLocation);
	void ReportHeard(const FVector& Location);
	/** Path following reached a nav link whose structural owner does not let it through (UHollowPathFollowingComponent). */
	void ReportObstacle(AActor* InObstacle);

	/** Starts an activity, ending the current one; OnFinished runs later (never inside this call) unless ended first. */
	void BeginActivity(EHollowAIActivity NewActivity, FOnHollowActivityFinished OnFinished);
	/** Drops the current activity without finishing it (its movement keeps going until the next one or StopMovement). */
	void EndActivity();
	/** Stops brain, activity and movement for good (death). */
	void Shutdown();

	/** Hollow.State.Stunned on its pawn (a pulse mine): it stands still and strikes at nothing. */
	bool IsStunned() const;
	/** How far it hears now (the definition's, unless a child scales it). */
	float GetHearingRange() const;
	/** Living, player-controlled pawn other than its own: what it may hunt and what it hears. */
	bool IsCandidate(const AActor* Actor) const;

	/** One line for Hollow.AI.Dump and the smoke logs. */
	virtual FString Describe() const;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

	/** Runs NewActivity (already current); a child class adds its own and passes the rest to this one. */
	virtual void StartActivity(EHollowAIActivity NewActivity);
	/** The current activity's move ended (or it was already at the goal). */
	virtual void HandleActivityMove(bool bSuccess);
	virtual void HandleAbilityEnded(const FAbilityEndedData& Data);
	/** The current activity is over (finished or dropped), before the next one starts. */
	virtual void OnActivityEnded(EHollowAIActivity Ended) {}
	/** The pawn was just stunned (abilities already cancelled, movement stopped). */
	virtual void OnStunned() {}
	/** Whether a new sight or sound makes the brain pick again at once (the Pilgrim: always; the stalker only while it roams). */
	virtual bool ShouldReconsiderOnPerception() const { return true; }
	/** Hearing range from now on (perception is reconfigured; noises already heard stay). */
	void SetHearingRange(float Range);

	void Reconsider();
	void FinishActivity(bool bSuccess);
	void FinishActivityAfter(bool bSuccess, float Seconds);
	void ClearActivityTimer();
	bool MoveForActivity(const FVector& Goal, AActor* GoalActor, float AcceptanceRadius, float Speed);
	bool TriggerAttack(const FGameplayTag& Event, AActor* AttackTarget);
	bool IsAttackActive(const FGameplayTag& Event) const;
	/** Edge-to-edge distance in the plane between its capsule and Other's collision. */
	double GapTo(const AActor* Other) const;
	double Now() const;
	const UHollowSectorTopologySubsystem* GetTopology() const;

	UPROPERTY(VisibleAnywhere, Category = "Hollow|AI")
	TObjectPtr<UStateTreeAIComponent> Brain;

	UPROPERTY(VisibleAnywhere, Category = "Hollow|AI")
	TObjectPtr<UAIPerceptionComponent> Perception;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

	EHollowAIActivity Activity = EHollowAIActivity::None;
	FTimerHandle ActivityTimer;
	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystem;

private:
	UFUNCTION()
	void HandlePerception(AActor* Actor, FAIStimulus Stimulus);

	void HandleStunChanged(const FGameplayTag Tag, int32 Count);
	/** A heard stimulus from another room counts only within the noise's inter-room range (DA_NoiseConfig). */
	bool IsAudibleFromItsRoom(const FAIStimulus& Stimulus) const;
	void ConfigureSenses(const UHollowEnemyDefinition& Definition);
	void StopAtObstacle();
	void MarkTargetUnreachable();
	void ForgetLostTarget();
	void ForgetStimulus();

	void StartPatrol();
	void StartChase();
	void StartInvestigate();
	void StartAttack();
	void StartBreach();
	void StepBreach();
	void FinishInvestigation(bool bSuccess);
	bool FindPatrolPoint(FVector& OutPoint) const;

	mutable TWeakObjectPtr<APawn> Target;
	TArray<TWeakObjectPtr<APawn>> Seen;
	double TargetUnreachableUntil = 0.0;
	/** When the target went out of sight (it is still the target for TargetMemorySeconds). */
	double TargetLostTime = 0.0;
	FVector TargetLastSeen = FVector::ZeroVector;
	FTimerHandle ForgetTargetTimer;

	FVector StimulusLocation = FVector::ZeroVector;
	double StimulusTime = 0.0;
	bool bHasStimulus = false;

	TWeakObjectPtr<AActor> Obstacle;
	FVector Home = FVector::ZeroVector;

	FOnHollowActivityFinished ActivityFinished;
	FAIRequestID ActivityMove;
	FTimerHandle ReconsiderTimer;

	FDelegateHandle AbilityEndedHandle;
	FDelegateHandle StunHandle;
};
