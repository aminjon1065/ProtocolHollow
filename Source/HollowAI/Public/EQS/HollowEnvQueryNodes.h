#pragma once

#include "EnvironmentQuery/EnvQueryContext.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_ProjectedPoints.h"

#include "HollowEnvQueryNodes.generated.h"

/**
 * Points on a grid over the querier's focus room (AHollowStalkerController::GetFocusRoom, the sector topology),
 * WallMargin in from its walls, projected to the navmesh (ADR-0019).
 */
UCLASS(meta = (DisplayName = "Hollow: Focus Room Points"))
class HOLLOWAI_API UHollowEnvQueryGenerator_FocusRoomPoints : public UEnvQueryGenerator_ProjectedPoints
{
	GENERATED_BODY()

public:
	explicit UHollowEnvQueryGenerator_FocusRoomPoints(const FObjectInitializer& ObjectInitializer);

	virtual void GenerateItems(FEnvQueryInstance& QueryInstance) const override;

	UPROPERTY(EditDefaultsOnly, Category = "Generator", meta = (ClampMin = "10", Units = "Centimeters"))
	float PointSpacing = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Generator", meta = (ClampMin = "0", Units = "Centimeters"))
	float WallMargin = 0.f;
};

/**
 * Filters out points a lamp lights (IHollowLightSource cones with a clear line from the lamp), checked PointHeight
 * above the point: where a body standing there would be seen (ADR-0019).
 */
UCLASS(meta = (DisplayName = "Hollow: Out Of Light Cone"))
class HOLLOWAI_API UHollowEnvQueryTest_OutOfLightCone : public UEnvQueryTest
{
	GENERATED_BODY()

public:
	explicit UHollowEnvQueryTest_OutOfLightCone(const FObjectInitializer& ObjectInitializer);

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;
	virtual FText GetDescriptionTitle() const override;

	UPROPERTY(EditDefaultsOnly, Category = "Test", meta = (ClampMin = "0", Units = "Centimeters"))
	float PointHeight = 0.f;
};

/** Where the squad comes and goes in the focus room: its hottest portal, else the room's centre (ADR-0019). */
UCLASS(meta = (DisplayName = "Hollow: Focus Room Hot Portal"))
class HOLLOWAI_API UHollowEnvQueryContext_FocusRoomHotPortal : public UEnvQueryContext
{
	GENERATED_BODY()

public:
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};
