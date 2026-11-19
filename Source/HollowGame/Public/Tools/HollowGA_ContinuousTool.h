#pragma once

#include "Abilities/HollowGameplayAbility.h"
#include "ActiveGameplayEffectHandle.h"

#include "HollowGA_ContinuousTool.generated.h"

class UHollowToolDefinition;
struct FOnAttributeChangeData;

/**
 * Continuous tool (welder, plasma cutter): active while its input is held (Docs/Architecture/03 sec. 3.2).
 * Owner: predicted beam cue at once. Server: fuel drain effect, and a sample timer (ToolDefinition->SampleRateHz)
 * that traces from the pawn's view, keeps a sticky target, applies tool work for the server time elapsed, heats the
 * tool and reports noise. Overheat or empty fuel end it. Welder and cutter differ only in data (their definitions).
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
