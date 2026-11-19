#pragma once

#include "GameFramework/Character.h"

#include "HollowExoFrameCharacter.generated.h"

class UCameraComponent;
class UHollowCharacterMovementComponent;
class UHollowInputConfig;
class UInputMappingContext;
class USkeletalMeshComponent;
struct FInputActionValue;

/**
 * Player character (ExoFrame). First-person arms and the third-person body use the engine first-person rendering
 * (ADR-0011): the owner sees FirstPersonMesh with its own FOV, everyone else sees the character mesh.
 * Movement is UHollowCharacterMovementComponent; all tuning, meshes and offsets are data (BP_ExoFrame).
 * Reparented onto AHollowCharacterBase when the ability system arrives (S1-05).
 */
UCLASS()
class HOLLOWGAME_API AHollowExoFrameCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	explicit AHollowExoFrameCharacter(const FObjectInitializer& ObjectInitializer);

	UHollowCharacterMovementComponent* GetHollowMovement() const;
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

	/** Skeleton socket the first-person camera follows (head bob comes from the animation). */
	static const FName CameraSocketName;

protected:
	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	void HandleMove(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);
	void HandleJumpStarted();
	void HandleJumpCompleted();
	void HandleCrouchStarted();
	void HandleCrouchCompleted();
	void HandleSprintStarted();
	void HandleSprintCompleted();

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Character")
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Character")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Input")
	TObjectPtr<const UHollowInputConfig> InputConfig;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Input")
	TObjectPtr<const UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Input")
	int32 DefaultMappingPriority = 0;
};
