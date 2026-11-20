#pragma once

#include "Components/ActorComponent.h"
#include "Extraction/HollowRaidTypes.h"
#include "Raid/HollowRaidStateTypes.h"

#include "HollowRaidFlowComponent.generated.h"

class AController;
class AHollowExtractionZone;
class UHollowRaidStateComponent;

/**
 * Server-only raid flow on the raid game mode (ADR-0016): Assembling -> Deploying -> InRaid <-> Extraction -> Debrief.
 * Polls the extraction zone on a timer (no Tick), decides every player's outcome (alive in the zone at departure:
 * Extracted; outside: MissingInAction; killed: Dead; left: Abandoned), sends each player their FHollowRaidResult and
 * takes the squad back to the hub. All state the clients need is in UHollowRaidStateComponent.
 */
UCLASS(ClassGroup = (Hollow))
class HOLLOWGAME_API UHollowRaidFlowComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	explicit UHollowRaidFlowComponent(const FObjectInitializer& ObjectInitializer);

	/** A player died (their pawn reports it). */
	void HandlePlayerDied(AController* Player);

	/** A player left the raid (logout): they resolve their own escrow as Abandoned. */
	void HandlePlayerLeft(AController* Exiting);

	EHollowRaidOutcome GetOutcome(const AController* Player) const;
	const FGuid& GetRaidId() const { return RaidId; }

	/** Development: one line about the flow. */
	FString Describe() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleSectorReady(bool bReady);
	void StartRaid();
	void PollZone();
	void Depart();
	void EndRaid(const TCHAR* Why);
	void ReturnToHub();

	UHollowRaidStateComponent* GetStateComponent() const;
	AHollowExtractionZone* FindZone();
	/** Players still in the raid: no outcome yet and a pawn. */
	TArray<AController*> GetLivingPlayers() const;
	void UpdateState(EHollowRaidPhase Phase, float PhaseEndServerTime, int32 LivingInZone, int32 CoresInZone);

	FGuid RaidId;
	double StartTime = 0.0;
	TMap<TWeakObjectPtr<AController>, EHollowRaidOutcome> Outcomes;
	TArray<FHollowItemStack> Loot;
	TWeakObjectPtr<AHollowExtractionZone> Zone;
	FTimerHandle PhaseTimer;
	FTimerHandle PollTimer;
	FDelegateHandle ReadyHandle;
};
