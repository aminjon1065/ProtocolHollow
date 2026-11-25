#pragma once

#include "Net/Serialization/FastArraySerializer.h"

#include "HollowInventoryList.generated.h"

class UHollowInventoryComponent;
class UHollowItemDefinition;

/** One item of a raid inventory (ADR-0030): a struct, not a UObject (Docs/Architecture/02 sec. 2.5). */
USTRUCT()
struct HOLLOWECONOMY_API FHollowInventoryEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	/** A tiny primary data asset: replicated as a net GUID, loaded by name on the owner. */
	UPROPERTY()
	TObjectPtr<UHollowItemDefinition> Item;

	/** Held now; an entry used up to 0 stays (its abilities stay granted, their item cost fails). */
	UPROPERTY()
	int32 Count = 0;

	/** Brought into the raid from the profile (the accepted loadout, plus what was added since; not the ship's free items). */
	UPROPERTY()
	int32 Taken = 0;

	/** Picked up in the raid (GP-01b, ADR-0045): goes to the stash if the player extracts with it. */
	UPROPERTY()
	int32 Found = 0;
};

/** The entries of a UHollowInventoryComponent, delta-replicated (FFastArraySerializer) to the owner only. */
USTRUCT()
struct HOLLOWECONOMY_API FHollowInventoryList : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FHollowInventoryEntry> Entries;

	/** The component holding this list (set by it, not replicated): told when replicated entries change. */
	UHollowInventoryComponent* Owner = nullptr;

	/** Once per received change, after every add, change and remove of it. */
	void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FHollowInventoryEntry, FHollowInventoryList>(Entries, DeltaParms, *this);
	}
};

template<>
struct TStructOpsTypeTraits<FHollowInventoryList> : public TStructOpsTypeTraitsBase2<FHollowInventoryList>
{
	enum
	{
		WithNetDeltaSerializer = true
	};
};
