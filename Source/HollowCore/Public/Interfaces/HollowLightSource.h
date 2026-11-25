#pragma once

#include "UObject/Interface.h"

#include "HollowLightSource.generated.h"

class AActor;
class UWorld;

/** What a light cone is (GP-02): enemies fear them differently. */
enum class EHollowLightKind : uint8
{
	/** A helmet lamp: a Whisperer flinches and backs away, the Echo-Stalker keeps out of it. */
	Lamp,
	/** An Aether lantern's beam: it also exposes (GE_Occult_Expose, applied by the lantern itself). */
	Lantern
};

HOLLOWCORE_API const TCHAR* LexToString(EHollowLightKind Kind);

/** A light cone in world space. */
struct HOLLOWCORE_API FHollowLightCone
{
	FVector Origin = FVector::ZeroVector;
	/** Unit direction of the cone's axis. */
	FVector Direction = FVector::ForwardVector;
	float HalfAngleDegrees = 0.f;
	float Range = 0.f;
	EHollowLightKind Kind = EHollowLightKind::Lamp;

	/** Inside the cone (ignores occlusion). */
	bool Contains(const FVector& Point) const;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UHollowLightSource : public UInterface
{
	GENERATED_BODY()
};

/**
 * Something that lights the world where the AI cares (ADR-0019, GP-02 ADR-0047): a player's helmet lamp and, while it
 * burns, the Aether lantern's beam. The Echo-Stalker keeps out of these cones (EQS UHollowEnvQueryTest_OutOfLightCone);
 * Whisperers flinch in them and flank through the dark between them (UHollowSwarmSubsystem). Server truth for the AI
 * (a lantern's beam is known where its ability runs).
 */
class HOLLOWCORE_API IHollowLightSource
{
	GENERATED_BODY()

public:
	/** Appends every cone it lights now (none when its lights are off). */
	virtual void GetLightCones(TArray<FHollowLightCone>& OutCones) const = 0;

	/** Every cone of every light source pawn in World, with the pawn that casts it (occlusion traces ignore it). */
	static void GatherCones(const UWorld* World, TArray<TPair<FHollowLightCone, const AActor*>>& OutCones);
};
