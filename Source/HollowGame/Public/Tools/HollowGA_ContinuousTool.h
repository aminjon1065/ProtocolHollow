#pragma once

#include "Abilities/HollowGameplayAbility.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayTagContainer.h"

#include "HollowGA_ContinuousTool.generated.h"

class APawn;
class UHollowToolDefinition;
struct FOnAttributeChangeData;

/**
 * Continuous tool (welder, plasma cutter, Aether lantern): active while its input is held (Docs/Architecture/03
 * sec. 3.2). Owner: predicted cue at once (GetCueTag: the beam). Server: fuel drain effect, and a sample timer
 * (ToolDefinition->SampleRateHz) that does the sample's work (ApplySample: the base traces from the pawn's view, keeps a
 * sticky target and applies tool work for the server time elapsed), then heats the tool and reports noise. Overheat or
 * empty fuel end it. One continuous tool runs at a time (Hollow.State.Tool.Active blocks the others, S6-02). Welder and
 * cutter differ only in data (their definitions); the lantern changes the sample (UHollowGA_AetherLantern).
 */
UCLASS(Abstract)
class HOLLOWGAME_API UHollowGA_ContinuousTool : public UHollowGameplayAbility
{
	GENERATED_BODY()

public:
	explicit UHollowGA_ContinuousTool(const FObjectInitializer& ObjectInitializer);

	const UHollowToolDefinition* GetToolDefinition() const { return ToolDefinition; }

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/**
	 * Server: one sample's work. Eyes and Direction are the wielder's view, DeltaSeconds the server time it stands for
	 * (at most two samples). Heat and noise follow in the base.
	 */
	virtual void ApplySample(APawn& Wielder, const FVector& Eyes, const FVector& Direction, float DeltaSeconds);

	/** The cue every machine shows while the tool runs (GameplayCue.Hollow.Tool.Beam). */
	virtual FGameplayTag GetCueTag() const;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Tool")
	TObjectPtr<UHollowToolDefinition> ToolDefinition;

private:
	void StartServerWork();
	void SampleTool();
	void HeatUp();
	void HandleFuelChanged(const FOnAttributeChangeData& Change);
	void EndFromServer();

	FTimerHandle SampleTimer;
	FActiveGameplayEffectHandle FuelDrain;
	FDelegateHandle FuelChangedHandle;

	TWeakObjectPtr<AActor> CurrentTarget;
	FVector LastImpactPoint = FVector::ZeroVector;
	FVector LastImpactNormal = FVector::ZeroVector;
	double LastHitTime = 0.0;
	double LastSampleTime = 0.0;
	int32 SampleCount = 0;
};
