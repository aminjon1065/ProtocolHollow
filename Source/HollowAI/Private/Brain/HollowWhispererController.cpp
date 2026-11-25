#include "Brain/HollowWhispererController.h"

#include "AbilitySystemComponent.h"
#include "Enemies/HollowWhispererDefinition.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "GameFramework/Pawn.h"
#include "HollowAILog.h"
#include "NavigationSystem.h"
#include "Tags/HollowGameplayTags.h"
#include "Threat/HollowSwarmRules.h"
#include "Threat/HollowSwarmSubsystem.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowWhispererController)

AHollowWhispererController::AHollowWhispererController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

const UHollowWhispererDefinition* AHollowWhispererController::GetWhispererDefinition() const
{
	return Cast<UHollowWhispererDefinition>(GetDefinition());
}

void AHollowWhispererController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (UAbilitySystemComponent* Abilities = BoundAbilitySystem.Get())
	{
		LitHandle = Abilities->RegisterGameplayTagEvent(HollowTags::State_Lit, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleLitChanged);
	}
	if (UHollowSwarmSubsystem* Swarm = UHollowSwarmSubsystem::Get(this))
	{
		Swarm->Register(*this);
	}
}

void AHollowWhispererController::OnUnPossess()
{
	if (UAbilitySystemComponent* Abilities = BoundAbilitySystem.Get())
	{
		Abilities->UnregisterGameplayTagEvent(LitHandle, HollowTags::State_Lit, EGameplayTagEventType::NewOrRemoved);
	}
	if (UHollowSwarmSubsystem* Swarm = UHollowSwarmSubsystem::Get(this))
	{
		Swarm->Unregister(*this);
	}
	Super::OnUnPossess();
}

// ---------------------------------------------------------------------------------------------------- facts

bool AHollowWhispererController::IsLit() const
{
	const UAbilitySystemComponent* Abilities = BoundAbilitySystem.Get();
	return Abilities && Abilities->HasMatchingGameplayTag(HollowTags::State_Lit);
}

bool AHollowWhispererController::IsFrenzied() const
{
	const UAbilitySystemComponent* Abilities = BoundAbilitySystem.Get();
	return Abilities && Abilities->HasMatchingGameplayTag(HollowTags::State_Frenzy);
}

void AHollowWhispererController::NoteLight(const FHollowLightCone& Cone)
{
	LastLightOrigin = Cone.Origin;
	bHasLight = true;
}

bool AHollowWhispererController::GetLastLight(FVector& OutOrigin) const
{
	OutOrigin = LastLightOrigin;
	return bHasLight;
}

bool AHollowWhispererController::PreyHasLight() const
{
	return UHollowSwarmSubsystem::HasLight(GetTarget());
}

bool AHollowWhispererController::CanBite() const
{
	const UHollowSwarmSubsystem* Swarm = UHollowSwarmSubsystem::Get(this);
	const APawn* Prey = GetTarget();
	return Swarm ? Swarm->CanBite(*this, Prey) : Prey != nullptr;
}

bool AHollowWhispererController::ShouldFlank() const
{
	if (!GetTarget())
	{
		return false;
	}
	if (!CanBite())
	{
		return true;   // the others bite it: it waits its turn in the dark
	}
	return !IsFrenzied() && PreyHasLight() && Now() >= LungeUntil;
}

double AHollowWhispererController::TargetCost(const APawn& Candidate) const
{
	const UHollowWhispererDefinition* Definition = GetWhispererDefinition();
	return HollowSwarmRules::PreyCost(Super::TargetCost(Candidate), UHollowSwarmSubsystem::HasLight(&Candidate), Definition ? Definition->LitPreyDistanceScale : 1.0);
}

void AHollowWhispererController::OnNewTarget(APawn& NewTarget)
{
	if (UHollowSwarmSubsystem* Swarm = UHollowSwarmSubsystem::Get(this))
	{
		Swarm->ReportSighting(*this, NewTarget);
	}
}

// ---------------------------------------------------------------------------------------------------- the light

void AHollowWhispererController::HandleLitChanged(const FGameplayTag Tag, const int32 Count)
{
	if (Count > 0)
	{
		ArmRecoil();
	}
}

void AHollowWhispererController::OnExposed()
{
	ArmRecoil();
}

void AHollowWhispererController::ArmRecoil()
{
	if (Now() < RecoilReadyAt || IsStunned() || !GetPawn())
	{
		return;
	}
	bRecoilPending = true;
	Reconsider();
}

void AHollowWhispererController::StartRecoil()
{
	const UHollowWhispererDefinition* Definition = GetWhispererDefinition();
	bRecoilPending = false;
	if (!Definition)
	{
		FinishActivityAfter(true, UE_KINDA_SMALL_NUMBER);
		return;
	}
	RecoilReadyAt = Now() + Definition->RecoilCooldownSeconds;
	++Recoils;
	StopMovement();
	if (UAbilitySystemComponent* Abilities = BoundAbilitySystem.Get())
	{
		const FGameplayTagContainer Attacks(HollowTags::Ability_Enemy);
		Abilities->CancelAbilities(&Attacks);   // a bite being wound up never lands
	}
	UE_LOG(LogHollowAI, Log, TEXT("%s: flinches in the %s"), *GetNameSafe(GetPawn()), IsExposed() ? TEXT("Aether light") : TEXT("light"));
	GetWorldTimerManager().SetTimer(ActivityTimer, this, &ThisClass::BackAway, Definition->FlinchSeconds, /*bLoop=*/false);
}

void AHollowWhispererController::BackAway()
{
	const UHollowWhispererDefinition* Definition = GetWhispererDefinition();
	const APawn* Self = GetPawn();
	if (!Definition || !Self || Activity != EHollowAIActivity::Recoil)
	{
		FinishActivity(true);
		return;
	}
	const FVector From = Self->GetActorLocation();
	const FVector Origin = bHasLight ? LastLightOrigin : From - Self->GetActorForwardVector();
	const double Distance = IsExposed() ? Definition->ExposedFleeDistance : Definition->RecoilDistance;
	const FVector Goal = HollowSwarmRules::RecoilGoal(From, Origin, Distance);
	const UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	FNavLocation OnNavmesh;
	const FVector Extent(Distance * 0.5, Distance * 0.5, Self->GetSimpleCollisionHalfHeight() * 4.0);
	if (!Navigation || !Navigation->ProjectPointToNavigation(Goal, OnNavmesh, Extent, Navigation->GetNavDataForProps(Self->GetNavAgentPropertiesRef(), From))
		|| !MoveForActivity(OnNavmesh.Location, nullptr, Definition->MoveAcceptanceRadius, Definition->ChaseSpeed))
	{
		FinishActivity(true);   // nowhere to back away to: it has flinched, that is all
	}
}

// ---------------------------------------------------------------------------------------------------- the flank

void AHollowWhispererController::StartFlank()
{
	const UHollowWhispererDefinition* Definition = GetWhispererDefinition();
	if (!Definition || !Definition->FlankQuery || !GetTarget())
	{
		Lunge();
		return;
	}
	FEnvQueryRequest Request(Definition->FlankQuery, GetPawn());
	FlankQueryId = Request.Execute(EEnvQueryRunMode::SingleResult, this, &ThisClass::HandleFlankQuery);
	if (FlankQueryId == INDEX_NONE)
	{
		Lunge();
	}
}

void AHollowWhispererController::HandleFlankQuery(TSharedPtr<FEnvQueryResult> Result)
{
	const UHollowWhispererDefinition* Definition = GetWhispererDefinition();
	if (!Result.IsValid() || Result->QueryID != FlankQueryId || Activity != EHollowAIActivity::Flank || !Definition)
	{
		return;   // an answer for a flank that is over
	}
	FlankQueryId = INDEX_NONE;
	if (!Result->IsSuccessful() || Result->Items.IsEmpty())
	{
		UE_LOG(LogHollowAI, Log, TEXT("%s: no dark way round %s"), *GetNameSafe(GetPawn()), *GetNameSafe(GetTarget()));
		Lunge();
		return;
	}
	if (!MoveForActivity(Result->GetItemAsLocation(0), nullptr, Definition->MoveAcceptanceRadius, Definition->ChaseSpeed))
	{
		Lunge();
	}
}

void AHollowWhispererController::Lunge()
{
	const UHollowWhispererDefinition* Definition = GetWhispererDefinition();
	LungeUntil = Now() + (Definition ? Definition->LungeSeconds : 0.0);
	FinishActivityAfter(true, UE_KINDA_SMALL_NUMBER);
}

// ---------------------------------------------------------------------------------------------------- activities

void AHollowWhispererController::StartActivity(const EHollowAIActivity NewActivity)
{
	switch (NewActivity)
	{
	case EHollowAIActivity::Recoil:
		StartRecoil();
		break;
	case EHollowAIActivity::Flank:
		StartFlank();
		break;
	case EHollowAIActivity::Attack:
		if (UHollowSwarmSubsystem* Swarm = UHollowSwarmSubsystem::Get(this))
		{
			Swarm->TakeBiteSlot(*this, GetTarget());
		}
		Super::StartActivity(NewActivity);
		break;
	default:
		Super::StartActivity(NewActivity);
		break;
	}
}

void AHollowWhispererController::HandleActivityMove(const bool bSuccess)
{
	const UHollowWhispererDefinition* Definition = GetWhispererDefinition();
	switch (Activity)
	{
	case EHollowAIActivity::Recoil:
		FinishActivity(true);
		break;
	case EHollowAIActivity::Flank:
		if (!bSuccess || !Definition)
		{
			Lunge();
		}
		else if (CanBite())
		{
			LungeUntil = Now() + Definition->LungeSeconds;   // out of the dark, at it
			FinishActivity(true);
		}
		else
		{
			FinishActivityAfter(true, Definition->FlankPauseSeconds);   // the others bite: it waits its turn here
		}
		break;
	default:
		Super::HandleActivityMove(bSuccess);
		break;
	}
}

void AHollowWhispererController::OnActivityEnded(const EHollowAIActivity Ended)
{
	if (Ended == EHollowAIActivity::Attack)
	{
		if (UHollowSwarmSubsystem* Swarm = UHollowSwarmSubsystem::Get(this))
		{
			Swarm->ReleaseBiteSlot(*this);
		}
	}
	else if (Ended == EHollowAIActivity::Flank)
	{
		FlankQueryId = INDEX_NONE;
	}
}

FString AHollowWhispererController::Describe() const
{
	const UHollowSwarmSubsystem* Swarm = UHollowSwarmSubsystem::Get(this);
	return FString::Printf(TEXT("%s, whisperer%s%s, prey %s, group %d, recoils %d%s"), *Super::Describe(), IsLit() ? TEXT(", lit") : TEXT(""),
		IsFrenzied() ? TEXT(", frenzy") : TEXT(""), PreyHasLight() ? TEXT("lit") : (GetTarget() ? TEXT("dark") : TEXT("-")),
		Swarm ? Swarm->GetGroupOf(GetPawn()) : INDEX_NONE, Recoils, bRecoilPending ? TEXT(", flinch pending") : TEXT(""));
}
