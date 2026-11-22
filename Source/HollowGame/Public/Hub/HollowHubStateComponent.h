#pragma once

#include "Components/ActorComponent.h"
#include "UObject/PrimaryAssetId.h"

#include "HollowHubStateComponent.generated.h"

/**
 * The hub's shared state on the game state (ADR-0032): the contract the host picked, replicated to the squad so every
 * player's contracts screen shows it (S5-04). Only the server changes it, and only for the host
 * (AHollowPlayerController::Server_SelectContract). The raid launch carries it (HollowRaidLauncher::Travel).
 */
UCLASS(ClassGroup = (Hollow))
class HOLLOWGAME_API UHollowHubStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnHollowContractChanged, const FPrimaryAssetId&);

	explicit UHollowHubStateComponent(const FObjectInitializer& ObjectInitializer);

	/** The component on World's game state, if any. */
	static UHollowHubStateComponent* Get(const UWorld* World);

	const FPrimaryAssetId& GetSelectedContract() const { return SelectedContract; }

	/** Server: Contract is the squad's choice now. False (nothing changes) for anything that is not a contract. */
	bool SelectContract(const FPrimaryAssetId& Contract);

	/** The choice changed: on the server at once, on clients when it arrives. */
	FOnHollowContractChanged OnContractChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	/** Server: starts on the contract the travel URL names (back from a raid), else UHollowSectorSettings::DefaultContract. */
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnRep_SelectedContract();

	UPROPERTY(ReplicatedUsing = OnRep_SelectedContract)
	FPrimaryAssetId SelectedContract;
};
