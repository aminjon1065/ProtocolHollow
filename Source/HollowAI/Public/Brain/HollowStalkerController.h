#pragma once

#include "Brain/HollowAIController.h"
#include "Resonance/HollowResonanceTypes.h"
#include "Threat/HollowThreatDirectorSubsystem.h"

#include "HollowStalkerController.generated.h"

class AHollowEnemyCharacter;
class UHollowStalkerDefinition;
struct FEnvQueryResult;
struct FHollowSquadResonanceNetState;
class UHollowSquadResonanceComponent;

/**
 * Brain body of the Echo-Stalker (ADR-0019): the enemy controller plus its own activities, on the director's leash.
 * Track shadows the squad from a neighbouring room, Ambush waits in the hottest room nobody is in and strikes whoever
 * comes close, Retreat slips into the nearest vent and stays hidden at the far end until the director calls it back.
 * Spots are found by the focus query (EQS: the focus room, out of every lamp, near its hottest portal).
 */
UCLASS()
class HOLLOWAI_API AHollowStalkerController : public AHollowAIController
{
	GENERATED_BODY()

public:
	explicit AHollowStalkerController(const FObjectInitializer& ObjectInitializer);

	/** Room the focus query looks in (ambush or shadowing room), INDEX_NONE before the first. */
	int32 GetFocusRoom() const { return FocusRoom; }
	/** The activities set it before each query; tests and tools that run the query themselves set it here. */
	void SetFocusRoom(int32 Room) { FocusRoom = Room; }
	bool IsHunting() const;
	bool IsRetreating() const;

	virtual FString Describe() const override;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void StartActivity(EHollowAIActivity NewActivity) override;
	virtual void HandleActivityMove(bool bSuccess) override;
	virtual void HandleAbilityEnded(const FAbilityEndedData& Data) override;
	virtual void OnActivityEnded(EHollowAIActivity Ended) override;
	virtual void OnStunned() override;
	virtual bool ShouldReconsiderOnPerception() const override;

private:
	enum class EPhase : uint8
	{
		None,
		Searching,
		Moving,
		Waiting,
		Striking,
		ToVent,
		InVent
	};

	const UHollowStalkerDefinition* GetStalkerDefinition() const;
	AHollowEnemyCharacter* GetEnemy() const;
	void StartTrack();
	void StartAmbush();
	void StartRetreat();
	void RunFocusQuery(int32 Room);
	void HandleFocusQuery(TSharedPtr<FEnvQueryResult> Result);
	void CheckAmbush();
	void EnterVent();
	void FinishVentTrip();
	void LeaveVent();
	void HandleModeChanged(AHollowEnemyCharacter* Stalker, EHollowThreatMode Mode);
	/** Hearing range = definition x HearingResonanceCurve(squad Resonance). */
	void HandleSquadResonance(const FHollowSquadResonanceNetState& State);

	int32 FocusRoom = INDEX_NONE;
	int32 QueryId = INDEX_NONE;
	int32 ExitVent = INDEX_NONE;
	EPhase Phase = EPhase::None;
	FTimerHandle AmbushCheckTimer;
	/** The trip through the shaft; apart from the activity's timer, the brain may re-pick Retreat meanwhile. */
	FTimerHandle VentTripTimer;
	FDelegateHandle ModeHandle;
	TWeakObjectPtr<UHollowSquadResonanceComponent> SquadResonance;
	FDelegateHandle SquadHandle;
	/** Tier of the last hearing change (a new tier is logged, the steps between are verbose). */
	EHollowResonanceTier HearingTier = EHollowResonanceTier::Low;
};
