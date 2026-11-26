#pragma once

#include "Components/ActorComponent.h"
#include "Interaction/HollowInteractionRules.h"
#include "Interfaces/HollowInteractable.h"

#include "HollowInteractionComponent.generated.h"

/**
 * Player interaction (S1-06). On the locally controlled pawn a timer (UHollowInteractionSettings::FocusRateHz, no Tick)
 * sweeps the Interaction channel from the eyes and tracks the focused IHollowInteractable. RequestInteract asks the
 * server, which re-checks line of sight, range, rate and state before calling Interact (CLAUDE.md sec. 3).
 * Solo and listen-host players use the same path: their server RPC simply runs locally.
 */
UCLASS(ClassGroup = (Hollow), meta = (BlueprintSpawnableComponent))
class HOLLOWGAME_API UHollowInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnHollowFocusChanged, AActor* /*NewFocus*/, const FHollowInteractionInfo& /*Info*/);

	explicit UHollowInteractionComponent(const FObjectInitializer& ObjectInitializer);

	/** Starts or stops the local focus probe; the owning pawn toggles it with local control. */
	void SetLocalFocusEnabled(bool bEnabled);

	/** Asks the server to interact with the focused actor (local input). */
	void RequestInteract();

	AActor* GetFocusedActor() const { return FocusedActor.Get(); }

	/** Fired locally when the focus changes, or its prompt does (the solo drone: hand over, then take back; DRONE-01). */
	FOnHollowFocusChanged OnFocusChanged;

	/** Server-side facts for a request to interact with Target: line of sight, distance, rate, state (also for tests). */
	FHollowInteractionCheck MeasureRequest(AActor* Target) const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void UpdateFocus();
	AActor* ProbeFocus() const;
	void SetFocus(AActor* NewFocus);

	UFUNCTION(Server, Reliable)
	void Server_Interact(AActor* Target);

	FTimerHandle FocusTimer;
	TWeakObjectPtr<AActor> FocusedActor;
	/** The prompt last shown for FocusedActor. */
	FText FocusedPrompt;
	/** World time of the last accepted interaction (server). */
	TOptional<double> LastInteractionTime;

	friend class FHollowInteractionFocusTest;   // automation test driving the focus probe without a ticking world
};
