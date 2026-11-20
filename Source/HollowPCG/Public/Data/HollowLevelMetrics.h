#pragma once

#include "Engine/DeveloperSettings.h"

#include "HollowLevelMetrics.generated.h"

class UHollowLightBudgetConfig;

/**
 * Level metrics of the modular room kit (Docs/QA/LevelMetrics.md). Single source of the numbers used by the
 * sector generator, the room validator and the greybox builder (Build/Python/create_docks_greybox.py).
 * Integer uu only: the generator works on an integer grid (CLAUDE.md sec. 5). BlueprintType: read by editor Python.
 * Project Settings > Game > Hollow Level Metrics, stored in DefaultGame.ini.
 */
UCLASS(Config = Game, DefaultConfig, BlueprintType, meta = (DisplayName = "Hollow Level Metrics"))
class HOLLOWPCG_API UHollowLevelMetrics : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Edge of one grid cell (X and Y). Room footprints are whole cells. */
	UPROPERTY(Config, EditAnywhere, Category = "Grid", meta = (ClampMin = "1", Units = "Centimeters"))
	int32 CellSize = 0;

	/** Clear height of one storey: floor top to ceiling bottom. */
	UPROPERTY(Config, EditAnywhere, Category = "Grid", meta = (ClampMin = "1", Units = "Centimeters"))
	int32 CellHeight = 0;

	/** Wall thickness. Walls sit inside the room footprint, flush with its boundary. */
	UPROPERTY(Config, EditAnywhere, Category = "Grid", meta = (ClampMin = "1", Units = "Centimeters"))
	int32 WallThickness = 0;

	/** Floor and ceiling slab thickness. */
	UPROPERTY(Config, EditAnywhere, Category = "Grid", meta = (ClampMin = "1", Units = "Centimeters"))
	int32 SlabThickness = 0;

	/** Standard connector opening, centered on a cell edge. */
	UPROPERTY(Config, EditAnywhere, Category = "Connector", meta = (ClampMin = "1", Units = "Centimeters"))
	int32 DoorWidth = 0;

	UPROPERTY(Config, EditAnywhere, Category = "Connector", meta = (ClampMin = "1", Units = "Centimeters"))
	int32 DoorHeight = 0;

	/** Narrowest clear corridor width. */
	UPROPERTY(Config, EditAnywhere, Category = "Connector", meta = (ClampMin = "1", Units = "Centimeters"))
	int32 MinCorridorWidth = 0;

	/** Square vent hatch (Echo-Stalker only). */
	UPROPERTY(Config, EditAnywhere, Category = "Connector", meta = (ClampMin = "1", Units = "Centimeters"))
	int32 VentSize = 0;

	/** Largest walking agent that must pass every connector (Iron Pilgrim). */
	UPROPERTY(Config, EditAnywhere, Category = "Agents", meta = (ClampMin = "1", Units = "Centimeters"))
	int32 LargestAgentRadius = 0;

	UPROPERTY(Config, EditAnywhere, Category = "Agents", meta = (ClampMin = "1", Units = "Centimeters"))
	int32 LargestAgentHeight = 0;

	/** What a room may light (DA_LightBudget, S4-03, ADR-0021). */
	UPROPERTY(Config, EditAnywhere, Category = "Lighting")
	TSoftObjectPtr<UHollowLightBudgetConfig> LightBudget;
};
