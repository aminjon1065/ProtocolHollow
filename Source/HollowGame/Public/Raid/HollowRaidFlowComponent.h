#pragma once

#include "Components/ActorComponent.h"
#include "Extraction/HollowRaidTypes.h"
#include "Raid/HollowRaidStateTypes.h"

#include "HollowRaidFlowComponent.generated.h"

class AController;
class AHollowExtractionZone;
class AHollowPlayerController;
class UHollowRaidStateComponent;
struct FHollowLoadoutSnapshot;

/**
 * Server-only raid flow on the raid game mode (ADR-0016): Assembling -> Deploying -> InRaid <-> Extraction -> Debrief.
 * Polls the extraction zone on a timer (no Tick), decides every player's outcome (alive in the zone at departure:
 * Extracted; outside: MissingInAction; killed: Dead; left: Abandoned), sends each player their FHollowRaidResult and
 * takes the squad back to the hub. All state the clients need is in UHollowRaidStateComponent. It also takes every
 * player's loadout (ADR-0030): asked from their machine on arrival, accepted once while the raid assembles or deploys,
 * it becomes their raid inventory; what they used up goes into their result.
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

	/**
	 * Player arrived in the raid (login or seamless travel, the game mode's player initialization): its machine is asked
	 * for the loadout it takes into this raid; before the raid has its id (the host's own player after travel), BeginPlay
	 * asks. Only arrived players are asked: a client's controller carried over by travel is replaced on arrival.
	 */
	void HandlePlayerArrived(AHollowPlayerController* Player);

	/**
	 * Player's machine sent its loadout: accepted once per raid, for this raid, while it assembles or deploys; what
	 * HollowLoadoutRules accepts of it (soft: dropped slots are logged, nobody is kicked) becomes the player's
	 * inventory. False if it was refused.
	 */
	bool AcceptLoadout(AController* Player, const FHollowLoadoutSnapshot& Loadout);

	EHollowRaidOutcome GetOutcome(const AController* Player) const;
	const FGuid& GetRaidId() const { return RaidId; }

	/** The contract the hub sent the squad on (ADR-0032; the sector is still UHollowSectorSettings::RaidSector until S7-04). */
	void SetContract(const FPrimaryAssetId& InContract) { Contract = InContract; }
	const FPrimaryAssetId& GetContract() const { return Contract; }

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
	void RequestLoadout(AHollowPlayerController* Player);

	UHollowRaidStateComponent* GetStateComponent() const;
	AHollowExtractionZone* FindZone();
	/** Players still in the raid: no outcome yet and a pawn. */
	TArray<AController*> GetLivingPlayers() const;
	void UpdateState(EHollowRaidPhase Phase, float PhaseEndServerTime, int32 LivingInZone, int32 CoresInZone);

	FGuid RaidId;
	FPrimaryAssetId Contract;
	double StartTime = 0.0;
	TMap<TWeakObjectPtr<AController>, EHollowRaidOutcome> Outcomes;
	/** Players whose loadout this raid took. */
	TSet<TWeakObjectPtr<AController>> LoadoutsTaken;
	/** Players that arrived (asked for their loadout once the raid has its id). */
	TArray<TWeakObjectPtr<AHollowPlayerController>> Arrived;
	TArray<FHollowItemStack> Loot;
	TWeakObjectPtr<AHollowExtractionZone> Zone;
	FTimerHandle PhaseTimer;
	FTimerHandle PollTimer;
	FDelegateHandle ReadyHandle;
};
