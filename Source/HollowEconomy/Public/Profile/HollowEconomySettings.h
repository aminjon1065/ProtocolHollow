#pragma once

#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "Items/HollowItemTypes.h"

#include "HollowEconomySettings.generated.h"

class UHollowEconomyConfig;

/** Local profile settings (Project Settings > Game > Hollow Economy, DefaultGame.ini). */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Hollow Economy"))
class HOLLOWECONOMY_API UHollowEconomySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Save slot of the local profile (multi-process tests give each process its own with -ini). */
	UPROPERTY(Config, EditAnywhere, Category = "Profile")
	FString SaveSlotName;

	UPROPERTY(Config, EditAnywhere, Category = "Profile", meta = (ClampMin = "0"))
	int32 SaveUserIndex = 0;

	/** A new profile starts with this. */
	UPROPERTY(Config, EditAnywhere, Category = "New profile", meta = (ClampMin = "0"))
	int32 StartingCredits = 0;

	/**
	 * The class a new profile plays (ADR-0034): its loadout is that class's starting loadout. A profile from before
	 * classes plays it too, with the loadout it has.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "New profile", meta = (AllowedTypes = "HollowClass"))
	FPrimaryAssetId DefaultClass;

	/** A new profile's stash (ADR-0031): enough materials for a first craft. */
	UPROPERTY(Config, EditAnywhere, Category = "New profile")
	TArray<FHollowItemStack> StartingStash;

	/** The hub economy: shop, sell and insurance prices, stash size (DA_EconomyConfig). */
	UPROPERTY(Config, EditAnywhere, Category = "Hub")
	TSoftObjectPtr<UHollowEconomyConfig> EconomyConfig;

	/** The most loadout slots a raid accepts (ADR-0030): later ones are dropped. */
	UPROPERTY(Config, EditAnywhere, Category = "Loadout", meta = (ClampMin = "1"))
	int32 MaxLoadoutSlots = 1;

	/** Item types a loadout may take into a raid (Hollow.Item.Type.* children); anything else (an Aether core) is dropped. */
	UPROPERTY(Config, EditAnywhere, Category = "Loadout", meta = (Categories = "Hollow.Item.Type"))
	FGameplayTagContainer LoadoutItemTypes;

	/**
	 * How many different items of each equip slot one loadout holds (S6-04, ADR-0037): two tools on one input would both
	 * fire, two gadgets on G spend both. Ship upgrades add to them (ExtraEquipSlots). A slot not listed has no limit.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Loadout", meta = (TitleProperty = "Slot"))
	TArray<FHollowEquipSlotLimit> EquipSlotLimits;
};
