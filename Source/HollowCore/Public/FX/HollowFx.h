#pragma once

#include "Containers/ArrayView.h"
#include "Delegates/Delegate.h"
#include "Math/Transform.h"
#include "UObject/SoftObjectPtr.h"

#include "HollowFx.generated.h"

class UFXSystemAsset;
class UFXSystemComponent;
class USceneComponent;
class UWorld;
struct FStreamableHandle;

/**
 * A Niagara effect of the data (FAB-01, ADR-0051): what plays, what plays instead for a player who chose Reduced
 * Flashes, how large, where and how long. Soft references: the actor that plays it starts loading it with itself
 * (HollowFx::Preload) and never loads it on the frame it plays. Every machine plays it from replicated state (no RPC).
 */
USTRUCT(BlueprintType)
struct HOLLOWCORE_API FHollowFxSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX", meta = (AllowedClasses = "/Script/Niagara.NiagaraSystem"))
	TSoftObjectPtr<UFXSystemAsset> System;

	/** Played instead for a player who chose Reduced Flashes (a dimmer system); none: nothing plays for them. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX", meta = (AllowedClasses = "/Script/Niagara.NiagaraSystem"))
	TSoftObjectPtr<UFXSystemAsset> ReducedFlashesSystem;

	/** Uniform scale of the system (the Fab ones are authored some 3 m across). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX", meta = (ClampMin = "0.01"))
	float Scale = 1.f;

	/** Where it plays in the frame of what plays it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX")
	FVector Offset = FVector::ZeroVector;

	/** Seconds it spawns before it is stopped (its particles then die out); 0: until what plays it stops it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX", meta = (ClampMin = "0", Units = "Seconds"))
	float Seconds = 0.f;

	bool IsSet() const { return !System.IsNull(); }
};

namespace HollowFx
{
	/** The system this machine plays: the reduced one if the player chose Reduced Flashes (it may be none). */
	HOLLOWCORE_API TSoftObjectPtr<UFXSystemAsset> Chosen(const FHollowFxSpec& Spec);

	/**
	 * Starts loading what Specs play (both choices: the setting may change during the raid); OnLoaded runs once they are
	 * in memory (at once if they already are). Keep the handle as long as they may play; null when nothing is set.
	 */
	HOLLOWCORE_API TSharedPtr<FStreamableHandle> Preload(TConstArrayView<const FHollowFxSpec*> Specs, FSimpleDelegate OnLoaded);

	/**
	 * Plays Spec on Parent (it follows Parent) until Stop; Frame is where in Parent's space the spec's Offset and Scale
	 * apply (an altar's seat). Null on a dedicated server, when nothing plays for this player or the system is not loaded
	 * yet (Preload's OnLoaded is the moment to try again).
	 */
	HOLLOWCORE_API UFXSystemComponent* PlayAttached(const FHollowFxSpec& Spec, USceneComponent* Parent, const FTransform& Frame = FTransform::Identity);

	/** Plays Spec once at Where: stopped after Spec.Seconds, then returned to the pool; null as PlayAttached. */
	HOLLOWCORE_API UFXSystemComponent* PlayAt(const FHollowFxSpec& Spec, UWorld* World, const FTransform& Where);

	/** Stops an effect: it spawns no more, its particles die out, then it goes (null is fine). */
	HOLLOWCORE_API void Stop(UFXSystemComponent* Component);

#if !UE_BUILD_SHIPPING
	/**
	 * Every effect this machine starts, with the system chosen for its player, even where nothing can render (the
	 * headless tests: Niagara spawns no component without a renderer). Not in Shipping.
	 */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnHollowFxPlay, const FHollowFxSpec& /*Spec*/, const UFXSystemAsset* /*System*/);
	HOLLOWCORE_API FOnHollowFxPlay& OnPlay();
#endif
}
