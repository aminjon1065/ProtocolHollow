#pragma once

#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "Items/HollowItemTypes.h"

#include "HollowEconomySettings.generated.h"

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

	UPROPERTY(Config, EditAnywhere, Category = "New profile")
	TArray<FHollowLoadoutSlot> StartingLoadout;

	/** The most loadout slots a raid accepts (ADR-0030): later ones are dropped. */
	UPROPERTY(Config, EditAnywhere, Category = "Loadout", meta = (ClampMin = "1"))
	int32 MaxLoadoutSlots = 1;

	/** Item types a loadout may take into a raid (Hollow.Item.Type.* children); anything else (an Aether core) is dropped. */
	UPROPERTY(Config, EditAnywhere, Category = "Loadout", meta = (Categories = "Hollow.Item.Type"))
	FGameplayTagContainer LoadoutItemTypes;
};
