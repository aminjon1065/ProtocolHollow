#pragma once

#include "Abilities/HollowAbilitySet.h"
#include "Abilities/HollowItemCostSource.h"
#include "Components/ActorComponent.h"
#include "Inventory/HollowInventoryList.h"
#include "Items/HollowItemTypes.h"

#include "HollowInventoryComponent.generated.h"

/**
 * A player's raid inventory (ADR-0030), on the player state: what the host accepted of the loadout the player's machine
 * sent (HollowLoadoutRules), changed only on the server, replicated to its owner only (FFastArraySerializer, push model).
 * Every item grants the abilities of its definition's AbilitySet on the owner's ability system while it is held, so
 * tools and consumables come strictly from the loadout; abilities that cost their item take it through
 * IHollowItemCostSource. The hub has none: the raid fills it. What was used up goes into the raid result (GetSpent).
 */
UCLASS(ClassGroup = (Hollow))
class HOLLOWECONOMY_API UHollowInventoryComponent : public UActorComponent, public IHollowItemCostSource
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE(FOnHollowInventoryChanged);

	explicit UHollowInventoryComponent(const FObjectInitializer& ObjectInitializer);

	static UHollowInventoryComponent* Get(const AActor* Owner);

	/**
	 * Server: what a raid accepted of a loadout (HollowLoadoutRules::Validate) becomes the inventory, replacing what was
	 * there; unknown items are skipped. Bonus (the ship's upgrades, ADR-0032) comes on top, free. Loads the items'
	 * ability sets synchronously (the raid is still assembling).
	 */
	void ResetFromLoadout(TConstArrayView<FHollowItemStack> Accepted, TConstArrayView<FHollowItemStack> Bonus = {});

	/**
	 * Server: Count more of Item (development now, loot later), granting its abilities if it is new. Not from the profile
	 * (a ship upgrade's free items): held, but not taken, so they are used first and never spent from the profile.
	 */
	void AddItem(UHollowItemDefinition* Item, int32 Count, bool bFromProfile = true);

	/**
	 * Server: Count of Item picked up in the raid (GP-01b, ADR-0045): held like the rest, and what of it is still held
	 * at the end goes to the stash if the player extracts (GetFound).
	 */
	void AddFoundItem(UHollowItemDefinition* Item, int32 Count);

	/** Server: empties the inventory and takes back what its items granted. */
	void Clear();

	int32 GetCount(const UHollowItemDefinition* Item) const;
	int32 GetCount(const FPrimaryAssetId& Item) const;
	const TArray<FHollowInventoryEntry>& GetEntries() const { return Inventory.Entries; }

	/** What was used up in the raid: taken minus held, per item. */
	TArray<FHollowItemStack> GetSpent() const;

	/** What of the found loot is still held (HollowLootRules::FoundHeld: the free items go first, then the found, then the own). */
	TArray<FHollowItemStack> GetFound() const;

	/** "DA_Item_PulseMine 1/2, ..." for dev dumps. */
	FString Describe() const;

	//~ IHollowItemCostSource
	virtual int32 GetItemCount(const UObject* Item) const override;
	virtual bool ConsumeItem(const UObject* Item, int32 Count) override;

	/** The contents changed: on the server at once, on the owning client when the change arrives. */
	FOnHollowInventoryChanged OnInventoryChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostInitProperties() override;

private:
	friend struct FHollowInventoryList;
	friend class FHollowLoadoutSyncInventoryTest;   // plays a received change on the owner's side

	FHollowInventoryEntry* FindEntry(const UObject* Item);
	const FHollowInventoryEntry* FindEntry(const UObject* Item) const;
	void Grant(UHollowItemDefinition* Item);
	void Changed(FHollowInventoryEntry* Entry);
	void HandleReplicatedChange();

	UPROPERTY(Replicated)
	FHollowInventoryList Inventory;

	/** Server: what each item's ability set granted, to take it back exactly. */
	TMap<FPrimaryAssetId, FHollowAbilitySetHandles> Granted;
};
