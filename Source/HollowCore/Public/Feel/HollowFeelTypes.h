#pragma once

#include "GameplayTagContainer.h"

#include "HollowFeelTypes.generated.h"

/**
 * A camera shake (GP-01c, ADR-0046): a few sines of their own phase per axis under an envelope that rises over BlendIn
 * and falls to nothing at Duration. Amplitudes are at scale 1.
 */
USTRUCT(BlueprintType)
struct HOLLOWCORE_API FHollowShakeParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shake", meta = (ClampMin = "0", Units = "Seconds"))
	float Duration = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shake", meta = (ClampMin = "0", Units = "Seconds"))
	float BlendIn = 0.f;

	/** Swing of the view's pitch, yaw and roll (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shake")
	FRotator Rotation = FRotator::ZeroRotator;

	/** Swing of the view's place, in its own forward, right and up (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shake")
	FVector Location = FVector::ZeroVector;

	/** How fast it swings. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shake", meta = (ClampMin = "0", Units = "Hertz"))
	float Frequency = 0.f;

	bool IsValid() const { return Duration > 0.f && Frequency > 0.f && (!Rotation.IsNearlyZero() || !Location.IsNearlyZero()); }
};

/** A pulse of the gamepad's motors (keyboard players feel nothing). */
USTRUCT(BlueprintType)
struct HOLLOWCORE_API FHollowRumble
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rumble", meta = (ClampMin = "0", ClampMax = "1"))
	float Intensity = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rumble", meta = (ClampMin = "0", Units = "Seconds"))
	float Duration = 0.f;

	/** The heavy motors (a thud) rather than the light ones (a buzz). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rumble")
	bool bHeavy = false;

	bool IsValid() const { return Intensity > 0.f && Duration > 0.f; }
};

/**
 * A kick of one player's view and of the first-person hands (UHollowViewFeelModifier): the view turns by Pitch and Yaw
 * (yaw to either side by the kick's serial), the hands come back by PushBack, then both spring back over Recover. Only
 * the camera moves: the aim (the control rotation) never does.
 */
USTRUCT(BlueprintType)
struct HOLLOWCORE_API FHollowViewKick
{
	GENERATED_BODY()

	/** Up for a recoil, down for a dip (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kick")
	float Pitch = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kick", meta = (ClampMin = "0"))
	float Yaw = 0.f;

	/** The hands towards the player (cm); negative thrusts them forward (a blow). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kick")
	float PushBack = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kick", meta = (ClampMin = "0", Units = "Seconds"))
	float Recover = 0.f;

	bool IsValid() const { return Recover > 0.f && (!FMath::IsNearlyZero(Pitch) || !FMath::IsNearlyZero(Yaw) || !FMath::IsNearlyZero(PushBack)); }
};

/**
 * What one trigger feels like. A Hollow.Feel.* trigger is emitted for one player (its own shot, the damage it takes);
 * a sound (Hollow.Noise.*, Hollow.Sound.*) is felt by every player near it when it plays (UHollowAudioSubsystem): full
 * within InnerRadius, fading to nothing at OuterRadius.
 */
USTRUCT(BlueprintType)
struct HOLLOWCORE_API FHollowFeelEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel", meta = (Categories = "Hollow.Feel,Hollow.Noise,Hollow.Sound"))
	FGameplayTag Trigger;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel")
	FHollowShakeParams Shake;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel")
	FHollowRumble Rumble;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel")
	FHollowViewKick Kick;

	/** A sound's: felt fully this close. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel", meta = (ClampMin = "0", Units = "Centimeters"))
	float InnerRadius = 0.f;

	/** A sound's: not felt from this far. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel", meta = (ClampMin = "0", Units = "Centimeters"))
	float OuterRadius = 0.f;

	/** Felt by one player only (a Hollow.Feel.* trigger), not by whoever is near. */
	bool IsPersonal() const;
};

/** Pure rules of game feel (Hollow.Core.Feel.Rules). */
namespace HollowFeelRules
{
	/** How strongly a sound's feel reaches Distance: 1 within InnerRadius, linear to 0 at OuterRadius, 0 beyond. A personal one: 1. */
	HOLLOWCORE_API float ScaleAt(const FHollowFeelEntry& Entry, float Distance);

	/** The shake's envelope at Time: rises over BlendIn, falls to 0 at Duration (quadratic, so it settles), 0 outside. */
	HOLLOWCORE_API float Envelope(const FHollowShakeParams& Shake, float Time);

	/** The shake's offset at Time for Seed (the phases of its sines): the same seed, the same shake. */
	HOLLOWCORE_API void Sample(const FHollowShakeParams& Shake, uint32 Seed, float Time, FRotator& OutRotation, FVector& OutLocation);

	/** A kick's share left Elapsed seconds after it: 1 at once, springing back to 0 at Recover (smooth). */
	HOLLOWCORE_API float KickLeft(const FHollowViewKick& Kick, float Elapsed);

	/** What breaks a config: duplicate triggers, an entry that does nothing, a sound's without a range, radii the wrong way round. */
	HOLLOWCORE_API TArray<FString> Check(TConstArrayView<FHollowFeelEntry> Entries);
}
