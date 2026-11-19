#pragma once

#include "Engine/DeveloperSettings.h"

#include "HollowNavSettings.generated.h"

class UNavigationQueryFilter;

/** Navigation defaults (Project Settings > Game > Hollow Navigation, DefaultGame.ini). */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Hollow Navigation"))
class HOLLOWPHYSICS_API UHollowNavSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Agents that open doors but never break them (Whisperers, Echo-Stalker): blocked by locked, welded, barricaded doors. */
	UPROPERTY(Config, EditAnywhere, Category = "Filters")
	TSoftClassPtr<UNavigationQueryFilter> StandardFilter;

	/** Breachers (Iron Pilgrim): pass every door, paying more for stronger ones. */
	UPROPERTY(Config, EditAnywhere, Category = "Filters")
	TSoftClassPtr<UNavigationQueryFilter> HeavyFilter;

	/** Nav agent names (DefaultEngine.ini SupportedAgents) that go with the filters. */
	UPROPERTY(Config, EditAnywhere, Category = "Agents")
	FName StandardAgent;

	UPROPERTY(Config, EditAnywhere, Category = "Agents")
	FName HeavyAgent;
};
