#pragma once

#include "Resonance/HollowResonanceTypes.h"
#include "Subsystems/WorldSubsystem.h"

#include "HollowResonancePresentationSubsystem.generated.h"

class AGameStateBase;
class UHollowResonancePresentationConfig;
class UHollowSquadResonanceComponent;
class UMaterialParameterCollection;
class USoundControlBus;

/**
 * The squad's Resonance on this machine's screen and speakers (ADR-0020), on every machine but a dedicated server.
 * Follows the replicated squad value (UHollowSquadResonanceComponent): eases the shown intensity towards it on a timer
 * that runs only while it moves, writes it and the server-time offset into MPC_Resonance (the post-process flickers
 * from Time + offset, so in phase everywhere) and sets the Resonance bus of Audio Modulation. No tick.
 */
UCLASS()
class HOLLOWGAME_API UHollowResonancePresentationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UHollowResonancePresentationSubsystem* Get(const UObject* WorldContext);

	float GetIntensity() const { return Intensity; }
	float GetTargetIntensity() const { return Target; }
	float GetBusValue() const;
	float GetTimeOffset() const { return TimeOffset; }
	/** Whether the shown value is still moving (the step timer runs only then). */
	bool IsEasing() const;
	const UHollowResonancePresentationConfig* GetConfig() const { return Config; }
	/** A scalar of the collection as this world holds it (0 without one). */
	float ReadCollectionScalar(FName Parameter) const;

	/** Follows Squad (world begin play finds the game state's; tests hand theirs). */
	void Attach(UHollowSquadResonanceComponent* InSquad);
	/** Presentation in use (UHollowResonancePresentationConfig::Get by default); tests swap in their own. */
	void SetConfig(const UHollowResonancePresentationConfig* InConfig);

	FString Describe() const;

protected:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
	void HandleGameStateSet(AGameStateBase* GameState);
	void Detach();
	void HandleSquadChanged(const FHollowSquadResonanceNetState& State);
	void Step();
	void Apply();

	UPROPERTY(Transient)
	TObjectPtr<const UHollowResonancePresentationConfig> Config;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialParameterCollection> CollectionAsset;

	UPROPERTY(Transient)
	TObjectPtr<USoundControlBus> Bus;

	TWeakObjectPtr<UHollowSquadResonanceComponent> Squad;
	FDelegateHandle SquadHandle;
	FDelegateHandle GameStateHandle;
	FTimerHandle StepTimer;
	float Intensity = 0.f;
	float Target = 0.f;
	float TimeOffset = 0.f;
	EHollowResonanceTier Tier = EHollowResonanceTier::Low;
};
