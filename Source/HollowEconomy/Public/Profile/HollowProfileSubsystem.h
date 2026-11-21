#pragma once

#include "Crafting/HollowCraftingRules.h"
#include "Engine/EngineBaseTypes.h"
#include "Profile/HollowProfileSource.h"
#include "Profile/HollowProfileTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "HollowProfileSubsystem.generated.h"

class UHollowProfileSaveGame;
class UNetDriver;

/**
 * The local player's profile (ADR-0005): owned by this machine, saved to a SaveGame slot. Runs the raid escrow
 * (ADR-0016) - written when this machine enters a raid, resolved by the host's result, by a lost connection to the
 * host (SignalLost) or, if found unresolved at startup or when another raid starts, as Abandoned (death rules).
 * A resolved raid stays pending until the debrief has been seen. The hub's screens use it as their profile source (S5-04).
 */
UCLASS()
class HOLLOWECONOMY_API UHollowProfileSubsystem : public UGameInstanceSubsystem, public IHollowProfileSource
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnHollowRaidReport, const FHollowRaidReport&);
	DECLARE_MULTICAST_DELEGATE(FOnHollowProfileChanged);

	static UHollowProfileSubsystem* Get(const UObject* WorldContext);

	/** Not in automation test worlds: tests never touch the player's real profile. */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	const FHollowProfile& GetProfile() const;
	const FHollowRaidEscrow& GetEscrow() const;
	/** The resolved raid the player has not seen yet, if any. */
	const FHollowRaidReport* GetPendingReport() const;

	/**
	 * What this player takes into raid RaidId (ADR-0030, the host asks while the raid assembles): the loadout's slots as
	 * they are (the host checks each one; what it will not take is logged here by the same rules). An unresolved escrow
	 * of another raid is first resolved as Abandoned, so nothing lost there can come along.
	 */
	TArray<FHollowItemStack> GetLoadoutForRaid(const FGuid& RaidId);

	/** This machine is in raid RaidId (every machine, the host included). */
	void BeginRaid(const FGuid& RaidId);

	/** The host's result for this player. */
	void ApplyRaidResult(const FHollowRaidResult& Result);

	/** The debrief has been seen. */
	void AcknowledgeReport();

	/**
	 * A transaction of the hub's shop, workbench or loadout locker (ADR-0031) on this machine's profile, under
	 * HollowCraftingRules with the project's terms; saved and announced when it applied. Anything else changed nothing.
	 */
	virtual EHollowTransactionResult ApplyTransaction(const FHollowProfileTransaction& Transaction) override;

	// IHollowProfileSource
	virtual const FHollowProfileState& GetProfileState() const override;
	virtual FOnProfileChanged& ProfileChanged() override { return OnProfileChanged; }

	/** Development: insure or uninsure a loadout slot (insurance UI comes later). False for a bad slot or an uninsurable item. */
	bool SetSlotInsured(int32 SlotIndex, bool bInsured);

	/**
	 * Development (crafting and the shop come later): Item's loadout slot holds Count (0 removes it; a new slot comes
	 * last). Insured only if the item can be. False for an unknown item.
	 */
	bool SetLoadoutSlot(const FPrimaryAssetId& Item, int32 Count, bool bInsured);

	/** Development: back to a new profile. */
	void ResetProfile();

	/** A raid was resolved (result, lost host, abandoned escrow). */
	FOnHollowRaidReport OnRaidReport;

	/** Credits, loadout or stash changed (a transaction, a raid's result, a development edit): the hub's screens refresh. */
	FOnHollowProfileChanged OnProfileChanged;

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
