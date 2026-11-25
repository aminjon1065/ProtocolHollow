#pragma once

#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"

#include "HollowFeelSubsystem.generated.h"

class AActor;
class APlayerController;
class UHollowFeelConfig;
class UHollowViewFeelModifier;
struct FHollowFeelEntry;

/**
 * Game feel on this machine (GP-01c, ADR-0046), on every machine but a dedicated server: camera shakes, gamepad
 * rumble and view kicks of its local players, from DA_FeelConfig. Purely local, nothing is sent. Two ways in: a player's
 * own feel (its shot, its blow, the damage it takes: EmitFor), and every sound UHollowAudioSubsystem plays, which its
 * local players near it feel by distance (a breach, a mine's blast, a Pilgrim's step, a crate breaking) - the moments
 * that already play their sound from replicated state on every machine, so the feel needs no hook of its own. With the
 * player's reduced flashes the shakes and kicks are softer (UHollowFeelConfig::ReducedFlashesScale). Never ticks.
 */
UCLASS()
class HOLLOWCORE_API UHollowFeelSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UHollowFeelSubsystem* Get(const UObject* WorldContext);

	/** One player's own feel (a Hollow.Feel.* trigger) for the local player whose pawn Viewer is. False if none here feels it. */
	bool EmitFor(const FGameplayTag& Trigger, const AActor* Viewer, float Scale = 1.f);

	/** A feel at Location (a sound's trigger): every local player within its radius feels it. Returns how many did. */
	int32 EmitAt(const FGameplayTag& Trigger, const FVector& Location, float Scale = 1.f);

	/** Player's view feel on its camera manager (added on first use); null without a camera manager. */
	UHollowViewFeelModifier* GetViewModifier(APlayerController& Player);

	/** The config in use (UHollowFeelConfig::Get by default); tests swap in their own. */
	void SetConfig(const UHollowFeelConfig* InConfig);
	const UHollowFeelConfig* GetConfig() const { return Config; }

	/** How strong every shake and kick is now: 1, or the config's reduced-flashes scale with the player's choice. */
	float GetComfortScale() const;

	/** How often Trigger was felt here (by any local player), and how strongly the last feel was. */
	int32 GetFeltCount(const FGameplayTag& Trigger) const;
	float GetLastScale() const { return LastScale; }
	void ResetCounts();

	FString Describe() const;

protected:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	void HandleSoundPlayed(const FGameplayTag& SoundTag, const FVector& Location, float VolumeScale);
	/** Player feels Entry at Scale (before comfort). */
	void Feel(APlayerController& Player, const FHollowFeelEntry& Entry, float Scale);

	UPROPERTY(Transient)
	TObjectPtr<const UHollowFeelConfig> Config;

	TMap<FGameplayTag, int32> Counts;
	FDelegateHandle SoundHandle;
	float LastScale = 0.f;
	uint32 ShakeSerial = 0;
};
