#include "ViewModels/HollowToolTargetModel.h"

#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Character/HollowExoFrameCharacter.h"
#include "Data/HollowToolDefinition.h"
#include "Engine/World.h"
#include "HollowUILog.h"
#include "Tags/HollowGameplayTags.h"
#include "TimerManager.h"
#include "Tools/HollowGA_ContinuousTool.h"
#include "Tools/HollowToolAim.h"
#include "ViewModels/HollowVisorContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowToolTargetModel)

void UHollowToolTargetModel::Initialize(UHollowVisorContext& InContext, const float InProbeRateHz, const float InLingerSeconds)
{
	Context = &InContext;
	ensureMsgf(InProbeRateHz > 0.f, TEXT("HollowUISettings.TargetProbeRateHz must be positive"));
	ProbeRateHz = FMath::Max(InProbeRateHz, UE_KINDA_SMALL_NUMBER);
	LingerSeconds = InLingerSeconds;
	SourcesHandle = InContext.OnSourcesChanged.AddUObject(this, &ThisClass::Rebind);
	Rebind();
}

void UHollowToolTargetModel::Deinitialize()
{
	StopProbing();
	if (UWorld* World = Context.IsValid() ? Context->GetWorld() : nullptr)
	{
		World->GetTimerManager().ClearTimer(LingerTimer);
	}
	if (UAbilitySystemComponent* Old = AbilitySystem.Get())
	{
		Old->RegisterGameplayTagEvent(HollowTags::State_Tool_Active, EGameplayTagEventType::NewOrRemoved).Remove(ToolTagHandle);
	}
	AbilitySystem.Reset();
	if (UHollowVisorContext* Owner = Context.Get())
	{
		Owner->OnSourcesChanged.Remove(SourcesHandle);
	}
	Context.Reset();
}

void UHollowToolTargetModel::Rebind()
{
	UAbilitySystemComponent* NewAbilitySystem = Context.IsValid() ? Context->GetAbilitySystem() : nullptr;
	if (AbilitySystem.Get() == NewAbilitySystem)
	{
		return;
	}
	if (UAbilitySystemComponent* Old = AbilitySystem.Get())
	{
		Old->RegisterGameplayTagEvent(HollowTags::State_Tool_Active, EGameplayTagEventType::NewOrRemoved).Remove(ToolTagHandle);
	}
	StopProbing();
	Hide();
	AbilitySystem = NewAbilitySystem;
	if (NewAbilitySystem)
	{
		ToolTagHandle = NewAbilitySystem->RegisterGameplayTagEvent(HollowTags::State_Tool_Active, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleToolActive);
		if (NewAbilitySystem->HasMatchingGameplayTag(HollowTags::State_Tool_Active))
		{
			StartProbing();
		}
	}
}

void UHollowToolTargetModel::HandleToolActive(const FGameplayTag Tag, const int32 Count)
{
	if (Count > 0)
	{
		StartProbing();
	}
	else if (bVisible)
	{
		StartLinger();   // probing goes on: the lingering bar still takes values that replicate late
	}
	else
	{
		StopProbing();
	}
}

void UHollowToolTargetModel::StartProbing()
{
	UWorld* World = Context.IsValid() ? Context->GetWorld() : nullptr;
	if (World && !World->GetTimerManager().IsTimerActive(ProbeTimer))
	{
		World->GetTimerManager().SetTimer(ProbeTimer, this, &ThisClass::Probe, 1.f / ProbeRateHz, /*bLoop=*/true);
		// GAS adds the owned tag before it counts the ability as active: look for the tool on the next frame.
		World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::Probe);
	}
}

void UHollowToolTargetModel::StopProbing()
{
	if (UWorld* World = Context.IsValid() ? Context->GetWorld() : nullptr)
	{
		World->GetTimerManager().ClearTimer(ProbeTimer);
	}
}

void UHollowToolTargetModel::Probe()
{
	const AHollowExoFrameCharacter* Character = Context.IsValid() ? Context->GetCharacter() : nullptr;
	const UHollowToolDefinition* Tool = FindActiveTool();
	FHollowTargetStatus NewStatus;
	if (!Tool)
	{
		// The tool stopped: keep reading the lingering target (no trace) until the linger ends.
		const IHollowTargetStatusSource* Lingering = Cast<IHollowTargetStatusSource>(Target.Get());
		if (bVisible && Lingering && Lingering->GetTargetStatus(ToolTag, NewStatus))
		{
			Publish(Target.Get(), NewStatus, ActionLabel);
		}
		return;
	}
	AActor* HitActor = nullptr;
	if (Character)
	{
		const FHollowToolAimTrace Trace = HollowToolAim::TraceLocal(*Character, *Tool);
		const IHollowTargetStatusSource* StatusTarget = Trace.bHit ? Cast<IHollowTargetStatusSource>(Trace.Hit.GetActor()) : nullptr;
		if (StatusTarget && StatusTarget->GetTargetStatus(Tool->ToolTag, NewStatus))
		{
			HitActor = Trace.Hit.GetActor();
		}
	}
	if (!HitActor)
	{
		if (bVisible)
		{
			StartLinger();   // slipped off: keep the last reading for a moment
		}
		return;
	}
	if (UWorld* World = Context->GetWorld())
	{
		World->GetTimerManager().ClearTimer(LingerTimer);
	}
	ToolTag = Tool->ToolTag;
	Publish(HitActor, NewStatus, Tool->ActionLabel);
}

void UHollowToolTargetModel::Publish(AActor* NewTarget, const FHollowTargetStatus& NewStatus, const FText& NewActionLabel)
{
	const bool bChanged = !bVisible || Target.Get() != NewTarget || NewStatus.Value != Status.Value || NewStatus.Marker != Status.Marker
		|| !ActionLabel.IdenticalTo(NewActionLabel);
	bVisible = true;
	Target = NewTarget;
	Status = NewStatus;
	ActionLabel = NewActionLabel;
	if (bChanged)
	{
		UE_LOG(LogHollowUI, Verbose, TEXT("Visor target %s: %s %.3f"), *GetNameSafe(NewTarget), *ActionLabel.ToString(), Status.Value);
		OnChanged.Broadcast();
	}
}

void UHollowToolTargetModel::StartLinger()
{
	UWorld* World = Context.IsValid() ? Context->GetWorld() : nullptr;
	if (!World)
	{
		Hide();
		return;
	}
	if (!World->GetTimerManager().IsTimerActive(LingerTimer))
	{
		World->GetTimerManager().SetTimer(LingerTimer, this, &ThisClass::Hide, FMath::Max(LingerSeconds, UE_KINDA_SMALL_NUMBER), /*bLoop=*/false);
	}
}

void UHollowToolTargetModel::Hide()
{
	const UAbilitySystemComponent* Source = AbilitySystem.Get();
	if (!Source || !Source->HasMatchingGameplayTag(HollowTags::State_Tool_Active))
	{
		StopProbing();   // the linger after the tool stopped is over
	}
	if (bVisible)
	{
		bVisible = false;
		Target.Reset();
		OnChanged.Broadcast();
	}
}

const UHollowToolDefinition* UHollowToolTargetModel::FindActiveTool() const
{
	const UAbilitySystemComponent* Source = AbilitySystem.Get();
	if (!Source)
	{
		return nullptr;
	}
	for (const FGameplayAbilitySpec& Spec : Source->GetActivatableAbilities())
	{
		const UHollowGA_ContinuousTool* Tool = Spec.IsActive() ? Cast<UHollowGA_ContinuousTool>(Spec.Ability) : nullptr;
		if (Tool && Tool->GetToolDefinition())
		{
			return Tool->GetToolDefinition();
		}
	}
	return nullptr;
}
