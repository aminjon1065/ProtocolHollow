#pragma once

#include "Components/GameStateComponent.h"
#include "Raid/HollowRaidStateTypes.h"

#include "HollowRaidStateComponent.generated.h"

/**
 * The raid as every machine sees it (ADR-0016), on the game state: phase, its end in server time, the extraction
 * countdown and who and what is in the zone. Only UHollowRaidFlowComponent changes it (push model). Every machine
 * puts its local profile into escrow as soon as the raid has an id and the pawns are coming (ADR-0005).
 */
UCLASS(ClassGroup = (Hollow))
class HOLLOWGAME_API UHollowRaidStateComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnHollowRaidStateChanged, const FHollowRaidNetState& /*Old*/, const FHollowRaidNetState& /*New*/);

	explicit UHollowRaidStateComponent(const FObjectInitializer& ObjectInitializer);

	static UHollowRaidStateComponent* Get(const UWorld* World);

	const FHollowRaidNetState& GetState() const { return State; }

	/** Server only. */
	void SetState(const FHollowRaidNetState& NewState);

	double GetServerTime() const;

	/** Seconds until PhaseEndServerTime (0 when untimed or past). */
	float GetSecondsLeft() const;

	FOnHollowRaidStateChanged OnStateChanged;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_State(const FHollowRaidNetState& OldState);

	void HandleChanged(const FHollowRaidNetState& OldState);

	UPROPERTY(ReplicatedUsing = OnRep_State)
	FHollowRaidNetState State;
};
