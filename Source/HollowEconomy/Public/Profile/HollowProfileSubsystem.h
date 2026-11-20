#pragma once

#include "Engine/EngineBaseTypes.h"
#include "Profile/HollowProfileTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "HollowProfileSubsystem.generated.h"

class UHollowProfileSaveGame;
class UNetDriver;

/**
 * The local player's profile (ADR-0005): owned by this machine, saved to a SaveGame slot. Runs the raid escrow
 * (ADR-0016) - written when this machine enters a raid, resolved by the host's result, by a lost connection to the
 * host (SignalLost) or, if found unresolved at startup or when another raid starts, as Abandoned (death rules).
 * A resolved raid stays pending until the debrief has been seen.
 */
UCLASS()
class HOLLOWECONOMY_API UHollowProfileSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnHollowRaidReport, const FHollowRaidReport&);

	static UHollowProfileSubsystem* Get(const UObject* WorldContext);

	/** Not in automation test worlds: tests never touch the player's real profile. */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	const FHollowProfile& GetProfile() const;
	const FHollowRaidEscrow& GetEscrow() const;
	/** The resolved raid the player has not seen yet, if any. */
	const FHollowRaidReport* GetPendingReport() const;

	/** This machine is in raid RaidId (every machine, the host included). */
	void BeginRaid(const FGuid& RaidId);

	/** The host's result for this player. */
	void ApplyRaidResult(const FHollowRaidResult& Result);

	/** The debrief has been seen. */
	void AcknowledgeReport();

	/** Development: insure or uninsure a loadout slot (insurance UI comes later). False for a bad slot or an uninsurable item. */
	bool SetSlotInsured(int32 SlotIndex, bool bInsured);

	/** Development: back to a new profile. */
	void ResetProfile();

	/** A raid was resolved (result, lost host, abandoned escrow). */
	FOnHollowRaidReport OnRaidReport;

private:
	void Load();
	void Persist() const;
	void MakeNewProfile();
	void HandleReport(const TOptional<FHollowRaidReport>& Report, const TCHAR* Why);
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);

	UPROPERTY(Transient)
	TObjectPtr<UHollowProfileSaveGame> Save;

	FDelegateHandle NetworkFailureHandle;
};
