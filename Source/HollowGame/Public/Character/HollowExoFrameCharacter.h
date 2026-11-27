#pragma once

#include "Abilities/HollowAbilitySet.h"
#include "Characters/HollowCharacterBase.h"
#include "Interfaces/HollowLightSource.h"

#include "GameplayTagContainer.h"

#include "HollowExoFrameCharacter.generated.h"

class UCameraComponent;
class AHollowPlayerState;
class UHollowClassComponent;
class UGameplayEffect;
class UHollowHandsComponent;
class UHollowHealthSet;
class UHollowInventoryComponent;
class UHollowCarrierComponent;
class UHollowFootstepAudioComponent;
class UHollowFootstepNoiseComponent;
class UHollowCharacterMovementComponent;
class UHollowInteractionComponent;
class UHollowToolMountComponent;
class UHollowViewFeelComponent;
class UHollowInputConfig;
class UInputMappingContext;
class USkeletalMeshComponent;
class USpotLightComponent;
class UStaticMeshComponent;
struct FInputActionValue;

/** The helmet lamp switched on or off on this machine (the visor's lamp line, S4-08). */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnHollowHelmetLampChanged, bool /*bOn*/);

/** The character died, seen on this machine (the server's Die, a client's OnRep_Dead): the solo drone goes dark (DRONE-01). */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnHollowCharacterDied, class AHollowExoFrameCharacter& /*Character*/);

/**
 * Player character (ExoFrame). First-person arms and the third-person body use the engine first-person rendering
 * (ADR-0011): the owner sees FirstPersonMesh with its own FOV, everyone else sees the character mesh. CHAR-01
 * (ADR-0055): the body is Paragon Wraith and the camera follows its head; first-person arms, when the character has
 * them, ride the camera (Wraith has no first-person poses: the owner's tool rides the camera, KnownIssues AK-4).
 * Movement is UHollowCharacterMovementComponent; all tuning, meshes and offsets are data (BP_ExoFrame).
 * The ability system lives on AHollowPlayerState; this pawn becomes its avatar while possessed, and the server grants
 * AbilitySets for exactly that time (attributes re-initialize on every spawn).
 * The helmet lamp is on at spawn and switches with GA_HelmetLamp (S4-08, ADR-0026); the Echo-Stalker keeps out of its
 * cone while it is lit.
 */
UCLASS()
class HOLLOWGAME_API AHollowExoFrameCharacter : public AHollowCharacterBase, public IHollowLightSource
{
	GENERATED_BODY()

public:
	explicit AHollowExoFrameCharacter(const FObjectInitializer& ObjectInitializer);

	UHollowCharacterMovementComponent* GetHollowMovement() const;
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }
	UHollowInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }
	UHollowCarrierComponent* GetCarrierComponent() const { return CarrierComponent; }
	UHollowFootstepNoiseComponent* GetFootstepNoise() const { return FootstepNoise; }
	USpotLightComponent* GetHelmetLight() const { return HelmetLight; }
	UHollowToolMountComponent* GetToolMount() const { return ToolMount; }
	UHollowHandsComponent* GetHands() const { return Hands; }
	UHollowViewFeelComponent* GetViewFeel() const { return ViewFeel; }

	/** The helmet lamp's cone from where the pawn looks while it is switched on and its wearer lives (any machine; the server's view drives the AI, ADR-0019). */
	bool GetHelmetLampCone(FHollowLightCone& OutCone) const;

	/** The AI's light (GP-02): the helmet lamp's cone and, while it burns, the Aether lantern's beam (where its ability runs). */
	virtual void GetLightCones(TArray<FHollowLightCone>& OutCones) const override;

	/** Whether the helmet lamp is switched on (a dead body's lamp stays dark whatever this says). */
	bool IsHelmetLampOn() const { return bHelmetLampOn; }

	/**
	 * Switches the helmet lamp on this machine (S4-08): GA_HelmetLamp calls it on the predicting owner and on the server.
	 * The server's value replicates to everyone but the owner, who predicts its own.
	 */
	void SetHelmetLampOn(bool bOn);

	FOnHollowHelmetLampChanged OnHelmetLampChanged;

	/** Every machine, once: the death presentation is applied. */
	FOnHollowCharacterDied OnDied;

	/** The Interact input: drops a carried core, otherwise interacts with the focus (a free core is predicted as lifted). */
	void RequestInteract();

	bool IsDead() const { return bDead; }

	/**
	 * Server: this ExoFrame dies (its health reached zero; ADR-0016). Movement stops, the carried core drops, the
	 * death effect marks Hollow.State.Dead, the raid learns it, and after a moment the player spectates.
	 */
	void Die();

	/** Socket of the body the first-person camera follows (head bob comes from the body's animation). */
	static const FName CameraSocketName;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostInitializeComponents() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void OnRep_PlayerState() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	/** Binds the player state's ability system to this pawn (server and owning client). */
	void InitializeFromPlayerState();

	/** Art-01: every machine follows the player's class: its colour on the helmet's visor, its tool in hand. */
	void BindClassLook(AHollowPlayerState* HollowPlayerState);
	void ApplyClassLook();

	void HandleOutOfHealth(AActor* EffectCauser, AActor* EffectInstigator, float Damage);
	void UnbindHealth();
	/** Server: the hands follow the raid inventory's weapon (GP-01). */
	void BindInventory(UHollowInventoryComponent* Inventory);
	void UnbindInventory();
	void BecomeSpectator();
	/** Every machine: a dead body neither moves nor blocks the living. */
	void ApplyDeadPresentation();
	/** The lamp casts shadows (if its data says so) only for the player of this machine, unless bOtherHelmetLampsCastShadows. */
	void ApplyHelmetShadows();

	/**
	 * UI-02 (ADR-0052): the camera of the player of this machine takes the field of view of its settings, now and
	 * whenever they change; every other copy keeps the data's.
	 */
	void ApplyFieldOfView();
	FDelegateHandle ControlsHandle;

	UFUNCTION()
	void OnRep_Dead();

	UFUNCTION()
	void OnRep_HelmetLamp();

	/** The lamp's light (and its click, for a switch seen on this machine) to match the switch and death. */
	void ApplyHelmetLamp(bool bClick);

	void HandleMove(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);
	void HandleJumpStarted();
	void HandleJumpCompleted();
	void HandleCrouchStarted();
	void HandleCrouchCompleted();
	void HandleSprintStarted();
	void HandleSprintCompleted();
	void HandleInteract();
	void HandleAbilityInputPressed(FGameplayTag InputTag);
	void HandleAbilityInputReleased(FGameplayTag InputTag);

	TWeakObjectPtr<UHollowClassComponent> LookClass;
	FDelegateHandle LookClassHandle;

	/** First-person arms on the camera, only the owner sees them (CHAR-01: none for Wraith, AK-4). */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Character")
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Character")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	/** On the camera: its cone, brightness, reach and shadows are data (BP_ExoFrame). */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Character")
	TObjectPtr<USpotLightComponent> HelmetLight;

	/** The tool in the first-person hands (ADR-0027): first-person rendering, only the owner sees it. */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Character")
	TObjectPtr<UStaticMeshComponent> FirstPersonToolMesh;

	/** The tool on the body: everyone else sees it, the owner its shadow. */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Character")
	TObjectPtr<UStaticMeshComponent> ToolMesh;

	/** Drives both tool meshes and tells where the beam leaves the tool; hand sockets and the default tool are data. */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Character")
	TObjectPtr<UHollowToolMountComponent> ToolMount;

	/** GP-01: tools or the weapon in hand (ADR-0044). */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Character")
	TObjectPtr<UHollowHandsComponent> Hands;

	/** GP-01c: its own player's view feel (the first-person tool's sway and recoil, the jolt of damage; ADR-0046). */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Character")
	TObjectPtr<UHollowViewFeelComponent> ViewFeel;

	/** The lamp's own data (BP_ExoFrame) before ApplyHelmetShadows. */
	bool bHelmetLampCastsShadows = false;

	/**
	 * Whether the lamps of the other players cast shadows on this machine (S4-05, Block 3 sec. 3.9): a moving shadowed light
	 * redraws its shadow pages every frame, and four of them would take the whole budget of a view. Off: only one's own does.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Character")
	bool bOtherHelmetLampsCastShadows = false;

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Character")
	TObjectPtr<UHollowInteractionComponent> InteractionComponent;

	/** Heavy carrying (ADR-0015): carried visuals attach to the camera (own view) and the body (everyone else). */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Character")
	TObjectPtr<UHollowCarrierComponent> CarrierComponent;

	/** Footsteps the AI hears (server; ADR-0018). */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Character")
	TObjectPtr<UHollowFootstepNoiseComponent> FootstepNoise;

	/** Footsteps players hear (every machine; the same cadence and loudness, ADR-0022). */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Character")
	TObjectPtr<UHollowFootstepAudioComponent> FootstepAudio;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Input")
	TObjectPtr<const UHollowInputConfig> InputConfig;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Input")
	TObjectPtr<const UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Input")
	int32 DefaultMappingPriority = 0;

	/** Abilities and effects granted by the server while this pawn is possessed (e.g. DA_AbilitySet_ExoFrame). */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Abilities")
	TArray<TObjectPtr<const UHollowAbilitySet>> AbilitySets;

	/** What AbilitySets granted, taken back on unpossess (server only). */
	FHollowAbilitySetHandles GrantedAbilitySetHandles;

	/** Applied by the server on death (GE_State_Dead: Hollow.State.Dead). */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Death")
	TSubclassOf<UGameplayEffect> DeathEffect;

	UPROPERTY(ReplicatedUsing = OnRep_Dead)
	bool bDead = false;

	/** Push model, to everyone but the owner: the owner predicts its own lamp (GA_HelmetLamp). On at spawn. */
	UPROPERTY(ReplicatedUsing = OnRep_HelmetLamp)
	bool bHelmetLampOn = true;

	TWeakObjectPtr<UHollowInventoryComponent> BoundInventory;
	FDelegateHandle InventoryChangedHandle;

	TWeakObjectPtr<const UHollowHealthSet> BoundHealthSet;
	FDelegateHandle OutOfHealthHandle;
	FTimerHandle SpectateTimer;
};
