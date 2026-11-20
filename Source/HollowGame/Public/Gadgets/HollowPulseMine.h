#pragma once

#include "GameFramework/Actor.h"

#include "HollowPulseMine.generated.h"

class AHollowEnemyCharacter;
class UHollowPulseMineDefinition;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * Kinetic pulse mine (ADR-0019), placed by UHollowGA_PulseMine. Server: arms after ArmSeconds, then checks on a timer
 * for an enemy within TriggerRadius; the blast stuns every enemy within BlastRadius in line of sight (the stun effect
 * from the placer's ability system), is a noise of the placer and flips the replicated bDetonated, so every machine
 * sees the flash. Gone FlashSeconds later. Players never set it off.
 */
UCLASS()
class HOLLOWGAME_API AHollowPulseMine : public AActor
{
	GENERATED_BODY()

public:
	explicit AHollowPulseMine(const FObjectInitializer& ObjectInitializer);

	const UHollowPulseMineDefinition* GetDefinition() const { return Definition; }
	bool IsArmed() const { return bArmed; }
	bool HasDetonated() const { return bDetonated; }

	/** Server: goes off now (the timer's check found an enemy; tests and dev call it directly). Returns how many it stunned. */
	int32 Detonate();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Mine")
	TObjectPtr<UStaticMeshComponent> Body;

	/** Off until the blast. */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Mine")
	TObjectPtr<UPointLightComponent> Flash;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Mine")
	TObjectPtr<const UHollowPulseMineDefinition> Definition;

private:
	void Arm();
	void CheckTrigger();
	/** Living enemies within Radius of the mine with nothing solid in between. */
	TArray<AHollowEnemyCharacter*> FindEnemies(float Radius) const;
	void ApplyDetonatedPresentation();

	UFUNCTION()
	void OnRep_Detonated();

	UPROPERTY(ReplicatedUsing = OnRep_Detonated)
	bool bDetonated = false;

	bool bArmed = false;
	FTimerHandle ArmTimer;
	FTimerHandle CheckTimer;
};
