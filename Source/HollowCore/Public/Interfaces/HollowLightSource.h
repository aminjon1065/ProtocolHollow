#pragma once

#include "UObject/Interface.h"

#include "HollowLightSource.generated.h"

/** A light cone in world space. */
struct HOLLOWCORE_API FHollowLightCone
{
	FVector Origin = FVector::ZeroVector;
	/** Unit direction of the cone's axis. */
	FVector Direction = FVector::ForwardVector;
	float HalfAngleDegrees = 0.f;
	float Range = 0.f;

	/** Inside the cone (ignores occlusion). */
	bool Contains(const FVector& Point) const;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UHollowLightSource : public UInterface
{
	GENERATED_BODY()
};

/**
 * Something that lights the world where the AI cares (ADR-0019): players' helmet lamps now. The Echo-Stalker keeps
 * out of these cones (EQS UHollowEnvQueryTest_OutOfLightCone). Any machine, from replicated state.
 */
class HOLLOWCORE_API IHollowLightSource
{
	GENERATED_BODY()

public:
	/** The cone it lights, if it is on. */
	virtual bool GetLightCone(FHollowLightCone& OutCone) const = 0;
};
