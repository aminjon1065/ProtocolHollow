#pragma once

#include "Abilities/HollowAbilitySet.h"
#include "Characters/HollowCharacterBase.h"

#include "GameplayTagContainer.h"

#include "HollowExoFrameCharacter.generated.h"

class UCameraComponent;
class UGameplayEffect;
class UHollowHealthSet;
class UHollowCarrierComponent;
class UHollowCharacterMovementComponent;
class UHollowInteractionComponent;
class UHollowInputConfig;
class UInputMappingContext;
class USkeletalMeshComponent;
struct FInputActionValue;

/**
 * Player character (ExoFrame). First-person arms and the third-person body use the engine first-person rendering
 * (ADR-0011): the owner sees FirstPersonMesh with its own FOV, everyone else sees the character mesh.
 * Movement is UHollowCharacterMovementComponent; all tuning, meshes and offsets are data (BP_ExoFrame).
 * The ability system lives on AHollowPlayerState; this pawn becomes its avatar while possessed, and the server grants
 * AbilitySets for exactly that time (attributes re-initialize on every spawn).
 */
UCLASS()
class HOLLOWGAME_API AHollowExoFrameCharacter : public AHollowCharacterBase
{
	GENERATED_BODY()

public:
	explicit AHollowExoFrameCharacter(const FObjectInitializer& ObjectInitializer);

	UHollowCharacterMovementComponent* GetHollowMovement() const;
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }
	UHollowInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }
	UHollowCarrierComponent* GetCarrierComponent() const { return CarrierComponent; }

	/** The Interact input: drops a carried core, otherwise interacts with the focus (a free core is predicted as lifted). */
	void RequestInteract();

	bool IsDead() const { return bDead; }

	/**
	 * Server: this ExoFrame dies (its health reached zero; ADR-0016). Movement stops, the carried core drops, the
	 * death effect marks Hollow.State.Dead, the raid learns it, and after a moment the player spectates.
	 */
	void Die();

	/** Skeleton socket the first-person camera follows (head bob comes from the animation). */
	static const FName CameraSocketName;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostInitializeComponents() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void OnRep_PlayerState() override;
	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	/** Binds the player state's ability system to this pawn (server and owning client). */
	void InitializeFromPlayerState();

	void HandleOutOfHealth(AActor* EffectCauser, AActor* EffectInstigator, float Damage);
	void UnbindHealth();
	void BecomeSpectator();
	/** Every machine: a dead body neither moves nor blocks the living. */
	void ApplyDeadPresentation();

	UFUNCTION()
	void OnRep_Dead();

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

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Character")
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Character")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Character")
	TObjectPtr<UHollowInteractionComponent> InteractionComponent;

	/** Heavy carrying (ADR-0015): carried visuals attach to the camera (own view) and the body (everyone else). */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Character")
	TObjectPtr<UHollowCarrierComponent> CarrierComponent;

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

	TWeakObjectPtr<const UHollowHealthSet> BoundHealthSet;
	FDelegateHandle OutOfHealthHandle;
	FTimerHandle SpectateTimer;
};
