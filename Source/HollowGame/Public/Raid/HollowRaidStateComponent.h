#pragma once

#include "Components/GameStateComponent.h"
#include "Raid/HollowRaidStateTypes.h"
#include "UObject/PrimaryAssetId.h"

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
	/** NAR-01b: a crew log reached this machine; bNew: this machine's profile did not have it (its bonus waits for the raid's end). */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnHollowLoreFound, const FPrimaryAssetId& /*Entry*/, bool /*bNew*/);

	explicit UHollowRaidStateComponent(const FObjectInitializer& ObjectInitializer);

	static UHollowRaidStateComponent* Get(const UWorld* World);

	const FHollowRaidNetState& GetState() const { return State; }

	/** Server only. */
	void SetState(const FHollowRaidNetState& NewState);

	double GetServerTime() const;

	/** Seconds until PhaseEndServerTime (0 when untimed or past). */
	float GetSecondsLeft() const;

	FOnHollowRaidStateChanged OnStateChanged;

	/** NAR-01b (ADR-0050): the crew logs the squad's datapads gave this raid, in order (a handful of ids). */
	const TArray<FPrimaryAssetId>& GetLoreFound() const { return LoreFound; }

	/**
	 * Server only (UHollowLoreSubsystem): a datapad gave the squad Entry. Every machine keeps it in its own profile as it
	 * arrives (ADR-0005: the profile is the player's) and tells OnLoreFound.
	 */
	void AddLoreFound(const FPrimaryAssetId& Entry);

	FOnHollowLoreFound OnLoreFound;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_State(const FHollowRaidNetState& OldState);

	void HandleChanged(const FHollowRaidNetState& OldState);

	UFUNCTION()
	void OnRep_LoreFound(const TArray<FPrimaryAssetId>& OldLore);

	/** Keeps Entry in this machine's profile and tells OnLoreFound. */
	void HandleLoreFound(const FPrimaryAssetId& Entry);

	UPROPERTY(ReplicatedUsing = OnRep_State)
	FHollowRaidNetState State;

	/** NAR-01b: replicated to the whole squad (the logs are shared), push model. */
	UPROPERTY(ReplicatedUsing = OnRep_LoreFound)
	TArray<FPrimaryAssetId> LoreFound;
};
