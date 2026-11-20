#include "Tools/HollowGA_ContinuousTool.h"

#include "AbilitySystemComponent.h"
#include "Attributes/HollowToolSet.h"
#include "Data/HollowToolDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "HollowGameLog.h"
#include "Interfaces/HollowToolTarget.h"
#include "Noise/HollowNoiseSubsystem.h"
#include "Tags/HollowGameplayTags.h"
#include "TimerManager.h"
#include "Tools/HollowToolTargeting.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowGA_ContinuousTool)

UHollowGA_ContinuousTool::UHollowGA_ContinuousTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EHollowAbilityActivationPolicy::WhileInputActive;
	ActivationBlockedTags.AddTag(HollowTags::State_Tool_Overheated);
	ActivationOwnedTags.AddTag(HollowTags::State_Tool_Active);   // local and immediate under prediction: the visor listens for it
}

bool UHollowGA_ContinuousTool::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags) || !ToolDefinition)
	{
		return false;
	}
	const UAbilitySystemComponent* AbilitySystem = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	return AbilitySystem && AbilitySystem->GetNumericAttribute(UHollowToolSet::GetToolFuelAttribute()) >= ToolDefinition->MinFuelToStart;
}

void UHollowGA_ContinuousTool::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	// Cosmetics start at once on the owner (predicted) and reach everyone else through the ability system.
	FGameplayCueParameters Cue;
	Cue.SourceObject = ToolDefinition;
	Cue.Instigator = GetAvatarActorFromActorInfo();
	Cue.EffectCauser = GetAvatarActorFromActorInfo();
	K2_AddGameplayCueWithParams(HollowTags::GameplayCue_Tool_Beam, Cue, /*bRemoveOnAbilityEnd=*/true);

	if (HasAuthority(&ActivationInfo))
	{
		StartServerWork();
	}
}

void UHollowGA_ContinuousTool::StartServerWork()
{
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	const UHollowToolDefinition* Tool = ToolDefinition;
	if (Tool->FuelDrainEffect)
	{
		const FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(Tool->FuelDrainEffect, GetAbilityLevel());
		const float Period = Tool->FuelDrainEffect.GetDefaultObject()->Period.GetValue();
		Spec.Data->SetSetByCallerMagnitude(HollowTags::Data_FuelCost, -Tool->FuelPerSecond * Period);
		FuelDrain = ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, Spec);
	}
	FuelChangedHandle = AbilitySystem->GetGameplayAttributeValueChangeDelegate(UHollowToolSet::GetToolFuelAttribute())
		.AddUObject(this, &ThisClass::HandleFuelChanged);

	CurrentTarget.Reset();
	SampleCount = 0;
	LastSampleTime = GetWorld()->GetTimeSeconds();
	GetWorld()->GetTimerManager().SetTimer(SampleTimer, this, &ThisClass::SampleTool, 1.f / Tool->SampleRateHz, /*bLoop=*/true);
	UE_LOG(LogHollowGame, Log, TEXT("%s: %s started (server)"), *GetNameSafe(GetAvatarActorFromActorInfo()), *Tool->ToolTag.ToString());
}

void UHollowGA_ContinuousTool::SampleTool()
{
	UWorld* World = GetWorld();
	APawn* Pawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	const UHollowToolDefinition* Tool = ToolDefinition;
	if (!Pawn || !Tool)
	{
		EndFromServer();
		return;
	}

	// Work follows server time; a hitch never turns into a burst bigger than two samples.
	const double Now = World->GetTimeSeconds();
	const float Delta = FMath::Clamp(static_cast<float>(Now - LastSampleTime), 0.f, 2.f / Tool->SampleRateHz);
	LastSampleTime = Now;
	++SampleCount;

	// The server already knows where the player looks (control rotation arrives with movement): no extra RPC.
	FVector Eyes;
	FRotator View;
	Pawn->GetActorEyesViewPoint(Eyes, View);
	const FVector Direction = View.Vector();
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(HollowToolSample), /*bTraceComplex=*/false, Pawn);
	FHitResult Hit;
	World->SweepSingleByChannel(Hit, Eyes, Eyes + Direction * Tool->Range, FQuat::Identity, Tool->TraceChannel,
		FCollisionShape::MakeSphere(Tool->HitToleranceRadius), Params);

	FHollowToolWork Work;
	Work.ToolTag = Tool->ToolTag;
	Work.Instigator = Pawn;
	Work.DeltaSeconds = Delta;
	Work.Amount = Tool->WorkRate * Delta;
	Work.ImpactPoint = Hit.ImpactPoint;
	Work.ImpactNormal = Hit.ImpactNormal;
	AActor* HitActor = Hit.GetActor();
	IHollowToolTarget* HitTarget = Cast<IHollowToolTarget>(HitActor);

	FHollowToolAim Aim;
	Aim.bHitTarget = HitTarget && HitTarget->CanAcceptToolWork(Work);
	Aim.bHasCurrent = CurrentTarget.IsValid();
	if (Aim.bHasCurrent)
	{
		const FVector ToImpact = (LastImpactPoint - Eyes).GetSafeNormal();
		Aim.AngleToCurrentDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Direction, ToImpact), -1.0, 1.0)));
		Aim.SecondsSinceCurrentHit = Now - LastHitTime;
	}

	switch (HollowToolTargeting::Resolve(Aim, Tool->StickyAngleDegrees, Tool->StickyGraceSeconds))
	{
	case EHollowToolAimResult::WorkOnHit:
		CurrentTarget = HitActor;
		LastImpactPoint = Work.ImpactPoint;
		LastImpactNormal = Work.ImpactNormal;
		LastHitTime = Now;
		HitTarget->ApplyToolWork(Work);
		break;
	case EHollowToolAimResult::KeepCurrent:
		if (IHollowToolTarget* Current = Cast<IHollowToolTarget>(CurrentTarget.Get()))
		{
			Work.ImpactPoint = LastImpactPoint;
			Work.ImpactNormal = LastImpactNormal;
			if (Current->CanAcceptToolWork(Work))
			{
				Current->ApplyToolWork(Work);
			}
		}
		break;
	case EHollowToolAimResult::Lose:
		CurrentTarget.Reset();
		break;
	}

	// The torch is loud whether or not it works on something; the noise is the wielder's (the impact point may lie in a
	// doorway's nav cut, on either side). Its rule limits the rate.
	if (UHollowNoiseSubsystem* Noise = UHollowNoiseSubsystem::Get(World); Noise && Tool->NoiseTag.IsValid())
	{
		Noise->ReportNoise(FHollowNoiseEvent(Tool->NoiseTag, Pawn->GetActorLocation(), Pawn));
	}

	HeatUp();
}

void UHollowGA_ContinuousTool::HeatUp()
{
	const UHollowToolDefinition* Tool = ToolDefinition;
	if (!Tool->HeatEffect)
	{
		return;
	}
	const FGameplayEffectSpecHandle Heat = MakeOutgoingGameplayEffectSpec(Tool->HeatEffect, GetAbilityLevel());
	Heat.Data->SetSetByCallerMagnitude(HollowTags::Data_HeatGain, Tool->HeatPerSample);
	ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, Heat);

	const UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	const float HeatValue = AbilitySystem->GetNumericAttribute(UHollowToolSet::GetToolHeatAttribute());
	const float MaxHeat = AbilitySystem->GetNumericAttribute(UHollowToolSet::GetMaxToolHeatAttribute());
	if (HeatValue >= MaxHeat && Tool->OverheatEffect)
	{
		UE_LOG(LogHollowGame, Log, TEXT("%s: %s overheated"), *GetNameSafe(GetAvatarActorFromActorInfo()), *Tool->ToolTag.ToString());
		const FGameplayEffectSpecHandle Overheat = MakeOutgoingGameplayEffectSpec(Tool->OverheatEffect, GetAbilityLevel());
		ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, Overheat);
		EndFromServer();
	}
}

void UHollowGA_ContinuousTool::HandleFuelChanged(const FOnAttributeChangeData& Change)
{
	if (Change.NewValue <= 0.f)
	{
		// Not from inside the attribute change: end on the next tick.
		GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::EndFromServer);
	}
}

void UHollowGA_ContinuousTool::EndFromServer()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
	}
}

void UHollowGA_ContinuousTool::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const bool bReplicateEndAbility, const bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SampleTimer);
	}
	if (UAbilitySystemComponent* AbilitySystem = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		if (FuelDrain.IsValid())
		{
			AbilitySystem->RemoveActiveGameplayEffect(FuelDrain);
			FuelDrain.Invalidate();
		}
		if (FuelChangedHandle.IsValid())
		{
			AbilitySystem->GetGameplayAttributeValueChangeDelegate(UHollowToolSet::GetToolFuelAttribute()).Remove(FuelChangedHandle);
			FuelChangedHandle.Reset();
			UE_LOG(LogHollowGame, Log, TEXT("%s: %s stopped after %d samples (server)"), *GetNameSafe(ActorInfo->AvatarActor.Get()),
				ToolDefinition ? *ToolDefinition->ToolTag.ToString() : TEXT("?"), SampleCount);
		}
	}
	CurrentTarget.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
