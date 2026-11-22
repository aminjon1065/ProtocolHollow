#pragma once

#include "GameFramework/Actor.h"

#include "HollowOccultWard.generated.h"

class APawn;
class AHollowEnemyCharacter;
class UAudioComponent;
class UDecalComponent;
class UHollowOccultWardDefinition;
class UMaterialInstanceDynamic;
class UPointLightComponent;

/**
 * The Techno-Occultist's ritual ward (S6-03, ADR-0036), laid by UHollowGA_OccultWard: a rune circle on the floor that
 * burns LifeSeconds. Server: every CheckSeconds it renews EnemyEffect on each living enemy inside (slowed) and
 * PlayerEffect plus a drain of Resonance on each living player inside; then it is gone. Every machine shows the circle
 * from the replicated server time it was laid (initial only): it comes up, burns, pulses before it goes out (timers, no
 * Tick), and hums for the players (no AI hears it). Nothing else is sent.
 */
UCLASS()
class HOLLOWGAME_API AHollowOccultWard : public AActor
{
	GENERATED_BODY()

public:
	explicit AHollowOccultWard(const FObjectInitializer& ObjectInitializer);

	const UHollowOccultWardDefinition* GetDefinition() const { return Definition; }
	UDecalComponent* GetRune() const { return Rune; }

	/** Seconds it still burns, as this machine sees it (from the replicated time it was laid). */
	float GetSecondsLeft() const;

	/** Inside the circle: within Radius of it on the floor plane, within HeightTolerance above or below the floor. */
	bool Contains(const FVector& Point) const;

	/** Server: looks who is inside now and renews their effects (its timer's work; tests call it directly). */
	void CheckInside();

	/** Server: the living enemies and players inside at the last check, by name. */
	const TArray<TWeakObjectPtr<AHollowEnemyCharacter>>& GetEnemiesInside() const { return EnemiesInside; }
	const TArray<TWeakObjectPtr<APawn>>& GetPlayersInside() const { return PlayersInside; }

	/** How bright the circle burns on this machine now, 0..1: coming up, steady, pulsing at the end. */
	float GetGlowAlpha() const;

	/** One line for Hollow.Gadget.Dump and the smoke logs. */
	FString Describe() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Ward")
	TObjectPtr<USceneComponent> Root;

	/** The rune circle projected onto the floor (its material: M_FX_RuneCircle). */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Ward")
	TObjectPtr<UDecalComponent> Rune;

	/** A small shadowless light over it. */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Ward")
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Ward")
	TObjectPtr<const UHollowOccultWardDefinition> Definition;

	/** Scalar parameter of the rune material that the glow scales (it comes up, it pulses). */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Ward")
	FName StrengthParameter;

	/** How often the look changes while the circle comes up or pulses. */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Ward", meta = (ClampMin = "1", Units = "Hertz"))
	float LookUpdateHz = 0.f;

private:
	double ServerNow() const;
	/** Seconds since it was laid, as this machine sees it. */
	double Age() const;
	void Expire();
	/** Shows the circle and keeps its look up to date: a timer while it changes, a wake-up for the pulse. */
	void UpdateLook();

	UFUNCTION()
	void OnRep_PlacedServerTime();

	/** Server time it was laid: every machine's clock for its look (and a late one's). */
	UPROPERTY(ReplicatedUsing = OnRep_PlacedServerTime)
	double PlacedServerTime = 0.0;

	TArray<TWeakObjectPtr<AHollowEnemyCharacter>> EnemiesInside;
	TArray<TWeakObjectPtr<APawn>> PlayersInside;
	/** Who was inside at the last check (logged when it changes). */
	FString LastInside;

	FTimerHandle LifeTimer;
	FTimerHandle CheckTimer;
	FTimerHandle LookTimer;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RuneMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Hum;

	/** The look's full brightness (the material's and the light's own values). */
	float RuneStrength = 0.f;
	float GlowIntensity = 0.f;
	bool bLookStarted = false;
};
