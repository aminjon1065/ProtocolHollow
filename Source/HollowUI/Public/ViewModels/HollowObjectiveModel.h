#pragma once

#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "Visor/HollowObjectiveRules.h"

#include "HollowObjectiveModel.generated.h"

class AActor;
class AHollowAetherCore;
class AHollowExtractionZone;
class AHollowHubTerminal;
class AHollowRitualAltar;
class APlayerController;
class IHollowProfileSource;
class UHollowAbilitySystemComponent;
class UHollowRaidStateComponent;
class UHollowVisorContext;
struct FHollowCarryNetState;
struct FHollowRaidNetState;

/** A marked place and where it is now. */
struct HOLLOWUI_API FHollowWaypointTarget
{
	EHollowWaypointKind Kind = EHollowWaypointKind::Extraction;
	FVector Location = FVector::ZeroVector;
};

/**
 * The visor's objective (UX-01, ADR-0042), owned by the visor context: it gathers the facts of HollowObjectiveRules
 * from events only, on every machine from what it already knows (the raid state, the cores' carry state and item,
 * the altars' phase, the hub terminals opened here, the player's death), and picks the step with its waypoints.
 * Nothing ticks; a timer redraws the shuttle's countdown once a second while the squad holds the zone.
 */
UCLASS()
class HOLLOWUI_API UHollowObjectiveModel : public UObject
{
	GENERATED_BODY()

public:
	/** The step, its line, its waypoints or whether a screen covers them changed. */
	DECLARE_MULTICAST_DELEGATE(FOnHollowObjectiveChanged);

	void Initialize(UHollowVisorContext& InContext);
	void Deinitialize();

	const FHollowObjective& GetObjective() const { return Objective; }
	const FHollowObjectiveFacts& GetFacts() const { return Facts; }

	/** The places the objective marks, where they are now (a carried core moves); the nearest to From of each kind. */
	void GetWaypoints(const FVector& From, TArray<FHollowWaypointTarget>& OutTargets) const;

	/** A hub terminal's screen is open on this machine: the waypoints step aside. */
	bool IsScreenOpen() const { return bScreenOpen; }

	/** Tests: prices come from this profile instead of the local player's (automation worlds have none). */
	void SetProfileSourceForTests(IHollowProfileSource* Source);

	/** Re-reads every fact now (tests after changing state by hand). */
	void Refresh();

	/** One line for Hollow.UI.Dump. */
	FString Describe() const;

	FOnHollowObjectiveChanged OnObjectiveChanged;

	virtual UWorld* GetWorld() const override;

private:
	struct FTrackedCore
	{
		TWeakObjectPtr<AHollowAetherCore> Core;
		/** Carried at least once this raid: it left the vault. */
		bool bLifted = false;
	};

	/** Binds the game state's raid state and the ability system (they come late on clients). */
	void BindSources();
	/** Follows Actor if the objective cares about it; true if it is new. */
	bool Track(AActor* Actor);
	void HandleActorSpawned(AActor* Actor);
	void HandleRaidState(const FHollowRaidNetState& OldState, const FHollowRaidNetState& NewState);
	void HandleCarry(const FHollowCarryNetState& OldState, const FHollowCarryNetState& NewState);
	void HandleTerminalOpened(APlayerController* Player, AHollowHubTerminal* Terminal);
	void HandleTerminalClosed(APlayerController* Player, AHollowHubTerminal* Terminal);
	void HandleDeadTag(FGameplayTag Tag, int32 Count);
	/** What a plain and a purified core bring this player home, once an altar says which items they are. */
	void UpdatePrices(const AHollowRitualAltar& Altar);
	void GatherFacts(FHollowObjectiveFacts& OutFacts);
	void UpdateCountdown();

	TWeakObjectPtr<UHollowVisorContext> Context;
	TWeakObjectPtr<UHollowRaidStateComponent> RaidState;
	TWeakObjectPtr<UHollowAbilitySystemComponent> AbilitySystem;
	TArray<FTrackedCore> Cores;
	TArray<TWeakObjectPtr<AHollowRitualAltar>> Altars;
	TArray<TWeakObjectPtr<AHollowExtractionZone>> Zones;
	TArray<TWeakObjectPtr<AHollowHubTerminal>> Terminals;

	FHollowObjectiveFacts Facts;
	FHollowObjective Objective;
	/** The raid the cores' lifted marks belong to. */
	FGuid RaidId;
	bool bPrepared = false;
	bool bScreenOpen = false;
	/** What the last OnObjectiveChanged told about the screen. */
	bool bAnnouncedScreenOpen = false;
	bool bPriced = false;
	IHollowProfileSource* TestSource = nullptr;

	FDelegateHandle SpawnedHandle;
	FDelegateHandle SourcesHandle;
	FDelegateHandle RaidHandle;
	FDelegateHandle DeadHandle;
	FDelegateHandle OpenedHandle;
	FDelegateHandle ClosedHandle;
	FTimerHandle CountdownTimer;
};
