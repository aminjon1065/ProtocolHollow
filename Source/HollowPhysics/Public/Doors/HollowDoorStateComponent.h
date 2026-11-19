#pragma once

#include "Components/ActorComponent.h"
#include "Doors/HollowDoorRules.h"
#include "GameplayTagContainer.h"

#include "HollowDoorStateComponent.generated.h"

class UHollowDoorDefinition;

/**
 * Replicated door state (Docs/Architecture/03 sec. 3.3). Changed only on the server through the transition table;
 * every machine follows NetState (push model; the owner is dormant between changes). Weld progress is throttled to
 * WeldSendInterval while a tool runs and the final value is sent when it stops.
 */
UCLASS(ClassGroup = (Hollow), meta = (BlueprintSpawnableComponent))
class HOLLOWPHYSICS_API UHollowDoorStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Old and new replicated state, on every machine; bFresh: a transition worth playing (not a late-join snap). */
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnHollowDoorStateChanged, const FHollowDoorNetState& /*Old*/, const FHollowDoorNetState& /*New*/, bool /*bFresh*/);

	explicit UHollowDoorStateComponent(const FObjectInitializer& ObjectInitializer);

	const UHollowDoorDefinition* GetDefinition() const { return Definition; }
	const FHollowDoorNetState& GetNetState() const { return NetState; }
	/** Exact on the server, from WeldQ elsewhere. */
	float GetWeld() const;
	/** Server time, the clock of every door transition. */
	double GetServerTime() const;

	/** Would the event be accepted now? (Prompts and tool stickiness; works on every machine from NetState.) */
	bool Accepts(EHollowDoorEvent Type, bool bDoorwayClear = true) const;

	/** Server only. */
	EHollowDoorOutcome Interact(bool bDoorwayClear);

	/** Server only: Weld or Cut event from a tool's work units. */
	EHollowDoorOutcome ApplyTool(EHollowDoorEvent Type, float WorkUnits);

	/** Which event a tool's work is for this door (Weld, Cut), if any. */
	TOptional<EHollowDoorEvent> ToolEvent(const FGameplayTag& ToolTag) const;

	FOnHollowDoorStateChanged OnStateChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, Category = "Hollow|Door")
	TObjectPtr<UHollowDoorDefinition> Definition;

private:
	EHollowDoorOutcome Handle(const FHollowDoorEvent& Event);
	void Publish(const FHollowDoorNetState& Old, bool bFresh);
	void SendWeldIfDue();
	void FlushWeld();
	void SendWeld();
	void FinishTransition();

	UFUNCTION()
	void OnRep_NetState(const FHollowDoorNetState& OldState);

	UPROPERTY(ReplicatedUsing = OnRep_NetState)
	FHollowDoorNetState NetState;

	/** Server: exact weld integrity behind NetState.WeldQ. */
	float Weld = 0.f;
	double LastWeldSendTime = 0.0;
	FTimerHandle TransitionTimer;
	FTimerHandle WeldFlushTimer;
};
