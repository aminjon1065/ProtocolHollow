#pragma once

#include "GameFramework/Actor.h"
#include "Interfaces/HollowCarryReceiver.h"
#include "Interfaces/HollowInteractable.h"

#include "HollowRitualAltar.generated.h"

class APawn;
class UFXSystemComponent;
struct FStreamableHandle;
class UHollowCarryableComponent;
class UHollowRitualAltarDefinition;
class UInstancedStaticMeshComponent;
class UPointLightComponent;

UENUM()
enum class EHollowAltarPhase : uint8
{
	/** Waiting for a core. */
	Idle,
	/** A core rests on the bowl, held; the bell tolls. */
	Ritual,
	/** The ritual is done: the core is purified and free again; one ritual a raid. */
	Spent
};

/** What every machine knows of the altar (push model, ADR-0039). */
USTRUCT()
struct FHollowAltarNetState
{
	GENERATED_BODY()

	UPROPERTY()
	EHollowAltarPhase Phase = EHollowAltarPhase::Idle;

	/** Server time the ritual started: every machine's clock for its tolls (and a late joiner's). */
	UPROPERTY()
	double RitualStartServerTime = 0.0;

	/** The object on the bowl. */
	UPROPERTY()
	TObjectPtr<AActor> Held = nullptr;
};

/**
 * The Purification Altar of the Reactor Cathedral (S7-02, ADR-0039), spawned by the server at an Altar socket. A
 * player carrying an Aether core hands it over with Interact (a carry receiver: the carrier focuses it, IHollowCarryReceiver):
 * the server sets the core down on the bowl, held, and runs a loud ritual for RitualSeconds: a bell tolls every
 * PulseSeconds (Hollow.Noise.Ritual, which the AI hears) and the room heats up at once for the Echo-Stalker. Then the
 * core counts as the result item (a purified core, worth more at extraction) and is free to lift again. Every machine
 * plays the tolls and the light from the replicated start time (timers, no Tick, no RPC).
 */
UCLASS()
class HOLLOWGAME_API AHollowRitualAltar : public AActor, public IHollowInteractable, public IHollowCarryReceiver
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnHollowAltarStateChanged, const AHollowRitualAltar& /*Altar*/);

	explicit AHollowRitualAltar(const FObjectInitializer& ObjectInitializer);

	const UHollowRitualAltarDefinition* GetDefinition() const { return Definition; }
	const FHollowAltarNetState& GetAltarState() const { return AltarState; }
	/** Seconds of the ritual left on this machine's clock (0 unless it runs). */
	float GetSecondsLeft() const;
	/** The seat in world space. */
	FTransform GetSeatTransform() const;
	/** Server: tolls so far (tests, logs). */
	int32 GetTolls() const { return Tolls; }
	/** One line for Hollow.Altar.Dump and the smoke logs. */
	FString Describe() const;

	/** This machine's ritual effect while it plays (FAB-01; tests). */
	const UFXSystemComponent* GetRitualEffect() const { return RitualEffect; }

	/** Every machine: the phase this machine shows changed (the visor's objective follows it, UX-01). */
	FOnHollowAltarStateChanged OnAltarStateChanged;

	// IHollowInteractable
	virtual FHollowInteractionInfo GetInteractionInfo(const AActor* Interactor) const override;
	virtual bool CanInteract(const AActor* Interactor) const override;
	virtual void Interact(AActor* Interactor) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Altar")
	TObjectPtr<USceneComponent> Root;

	/** The greybox altar (UHollowRitualAltarDefinition::Pieces): solid, the AI walks round it. */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Altar")
	TObjectPtr<UInstancedStaticMeshComponent> Pieces;

	/** A shadowless light over the bowl. */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Altar")
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Altar")
	TObjectPtr<const UHollowRitualAltarDefinition> Definition;

private:
	double ServerNow() const;
	/** The object Interactor carries (or null). */
	UHollowCarryableComponent* CarriedBy(const AActor* Interactor) const;
	/** Server: a toll's noise, attributed to the player who laid the core, or the living player nearest the altar. */
	void ReportToll();
	/** Server: the ritual is over: the core counts as the result and is free again. */
	void Complete();
	/** Server: replicate the state now. */
	void SetAltarState(const FHollowAltarNetState& NewState);
	/** Every machine: the look and the tolls follow the phase. */
	void ApplyState();
	/** Every machine: a toll now (bell, flash; the server's noise), the next one on a timer. */
	void Toll();
	void EndFlash();
	/** Every machine: the ritual's effect plays while the phase is Ritual (FAB-01), once its system is loaded. */
	void ApplyRitualFx();

	UFUNCTION()
	void OnRep_AltarState();

	UPROPERTY(ReplicatedUsing = OnRep_AltarState)
	FHollowAltarNetState AltarState;

	/** The phase this machine shows (the purified sound plays only on a ritual it saw end). */
	EHollowAltarPhase ShownPhase = EHollowAltarPhase::Idle;
	/** Server: who laid the core (the tolls are theirs while they live). */
	TWeakObjectPtr<APawn> Starter;
	int32 Tolls = 0;
	/** Index of the next toll on this machine's clock. */
	int32 NextToll = 0;
	float GlowIntensity = 0.f;

	FTimerHandle TollTimer;
	FTimerHandle FlashTimer;
	FTimerHandle CompleteTimer;
	UPROPERTY(Transient)
	TObjectPtr<UFXSystemComponent> RitualEffect;
	/** Keeps the altar's effects loaded while it stands. */
	TSharedPtr<FStreamableHandle> FxHandle;
};
