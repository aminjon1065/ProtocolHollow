#pragma once

#include "Engine/TimerHandle.h"
#include "GameplayTagContainer.h"
#include "Interfaces/HollowTargetStatusSource.h"
#include "UObject/Object.h"

#include "HollowToolTargetModel.generated.h"

class AActor;
class UAbilitySystemComponent;
class UHollowToolDefinition;
class UHollowVisorContext;

/**
 * Visor bar of the active tool's target (ADR-0013). Idle until the local ability system owns Hollow.State.Tool.Active;
 * then it traces like the beam (HollowToolAim) at TargetProbeRateHz and reads the hit IHollowTargetStatusSource, so it
 * shows replicated progress (a door's weld) on every machine. Lingers a moment after the tool stops or slips off, still
 * reading the same target, so a final value that replicates after the release still shows.
 */
UCLASS()
class HOLLOWUI_API UHollowToolTargetModel : public UObject
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE(FOnHollowToolTargetChanged);

	void Initialize(UHollowVisorContext& InContext, float InProbeRateHz, float InLingerSeconds);
	void Deinitialize();

	bool IsVisible() const { return bVisible; }
	const FHollowTargetStatus& GetStatus() const { return Status; }
	/** The tool's ActionLabel ("Welding"). */
	const FText& GetActionLabel() const { return ActionLabel; }
	AActor* GetTarget() const { return Target.Get(); }

	FOnHollowToolTargetChanged OnChanged;

private:
	void Rebind();
	void HandleToolActive(FGameplayTag Tag, int32 Count);
	void StartProbing();
	void StopProbing();
	void Probe();
	void Publish(AActor* NewTarget, const FHollowTargetStatus& NewStatus, const FText& NewActionLabel);
	void StartLinger();
	void Hide();
	const UHollowToolDefinition* FindActiveTool() const;

	TWeakObjectPtr<UHollowVisorContext> Context;
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	FDelegateHandle SourcesHandle;
	FDelegateHandle ToolTagHandle;
	FTimerHandle ProbeTimer;
	FTimerHandle LingerTimer;
	float ProbeRateHz = 0.f;
	float LingerSeconds = 0.f;

	bool bVisible = false;
	FHollowTargetStatus Status;
	FText ActionLabel;
	FGameplayTag ToolTag;
	TWeakObjectPtr<AActor> Target;
};
