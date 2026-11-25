#include "Threat/HollowSwarmSubsystem.h"

#include "AbilitySystemComponent.h"
#include "Brain/HollowWhispererController.h"
#include "CollisionQueryParams.h"
#include "Enemies/HollowEnemyCharacter.h"
#include "Enemies/HollowWhispererDefinition.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "HollowAILog.h"
#include "Threat/HollowSwarmRules.h"
#include "Threat/HollowThreatConfig.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowSwarmSubsystem)

namespace HollowSwarmPrivate
{
	void Renew(UAbilitySystemComponent& Abilities, const TSubclassOf<UGameplayEffect>& Effect)
	{
		if (Effect)
		{
			Abilities.ApplyGameplayEffectToSelf(Effect.GetDefaultObject(), 1.f, Abilities.MakeEffectContext());
		}
	}
}

UHollowSwarmSubsystem* UHollowSwarmSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UHollowSwarmSubsystem>() : nullptr;
}

bool UHollowSwarmSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UHollowSwarmSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Config = UHollowThreatConfig::Get();
}

void UHollowSwarmSubsystem::Deinitialize()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LightTimer);
	}
	Members.Reset();
	Groups.Reset();
	Super::Deinitialize();
}

void UHollowSwarmSubsystem::SetConfig(const UHollowThreatConfig* InConfig)
{
	Config = InConfig;
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LightTimer);   // restarted at the new pace
	}
	UpdateTimer();
}

UHollowSwarmSubsystem::FMember* UHollowSwarmSubsystem::Find(const AHollowWhispererController& Whisperer)
{
	return Members.FindByPredicate([&Whisperer](const FMember& Member) { return Member.Brain.Get() == &Whisperer; });
}

const UHollowSwarmSubsystem::FMember* UHollowSwarmSubsystem::Find(const AHollowWhispererController& Whisperer) const
{
	return Members.FindByPredicate([&Whisperer](const FMember& Member) { return Member.Brain.Get() == &Whisperer; });
}

int32 UHollowSwarmSubsystem::GetGroupOf(const APawn* Whisperer) const
{
	const TPair<TWeakObjectPtr<APawn>, int32>* Entry = Groups.FindByPredicate([Whisperer](const TPair<TWeakObjectPtr<APawn>, int32>& Pair)
	{
		return Whisperer && Pair.Key.Get() == Whisperer;
	});
	return Entry ? Entry->Value : INDEX_NONE;
}

void UHollowSwarmSubsystem::Register(AHollowWhispererController& Whisperer)
{
	if (Find(Whisperer))
	{
		return;
	}
	FMember& Member = Members.AddDefaulted_GetRef();
	Member.Brain = &Whisperer;
	Member.Body = Whisperer.GetPawn();
	Member.Group = GetGroupOf(Whisperer.GetPawn());
	UpdateTimer();
}

void UHollowSwarmSubsystem::Unregister(const AHollowWhispererController& Whisperer)
{
	Members.RemoveAll([&Whisperer](const FMember& Member) { return Member.Brain.Get() == &Whisperer || !Member.Brain.IsValid(); });
	UpdateTimer();
}

void UHollowSwarmSubsystem::RegisterBrood(const TConstArrayView<AHollowEnemyCharacter*> Brood)
{
	const int32 Group = NextGroup++;
	for (AHollowEnemyCharacter* Whisperer : Brood)
	{
		if (!Whisperer)
		{
			continue;
		}
		Groups.Emplace(Whisperer, Group);
		for (FMember& Member : Members)
		{
			if (Member.Body.Get() == Whisperer)
			{
				Member.Group = Group;
			}
		}
	}
	UE_LOG(LogHollowAI, Log, TEXT("Swarm: brood %d of %d"), Group, Brood.Num());
}

void UHollowSwarmSubsystem::ReportSighting(const AHollowWhispererController& Whisperer, const APawn& Prey)
{
	const FMember* Seer = Find(Whisperer);
	if (!Seer || Seer->Group == INDEX_NONE)
	{
		return;
	}
	for (const FMember& Member : Members)
	{
		AHollowWhispererController* Mate = Member.Brain.Get();
		if (Mate && Mate != &Whisperer && Member.Group == Seer->Group && !Mate->GetTarget())
		{
			Mate->ReportHeard(Prey.GetActorLocation());   // it comes to look; its own eyes take over there
		}
	}
}

int32 UHollowSwarmSubsystem::CountBiters(const APawn* Prey, const AHollowWhispererController* Except) const
{
	int32 Count = 0;
	for (const FMember& Member : Members)
	{
		if (Prey && Member.BitePrey.Get() == Prey && Member.Brain.Get() != Except && Member.Brain.IsValid())
		{
			++Count;
		}
	}
	return Count;
}

bool UHollowSwarmSubsystem::CanBite(const AHollowWhispererController& Whisperer, const APawn* Prey) const
{
	const FMember* Self = Find(Whisperer);
	if (!Prey || !Config)
	{
		return Prey != nullptr;
	}
	if (Self && Self->BitePrey.Get() == Prey)
	{
		return true;   // holds its slot already
	}
	return HollowSwarmRules::CanBite(CountBiters(Prey, &Whisperer), Config->Swarm.MaxBitersPerPrey, Whisperer.IsFrenzied());
}

void UHollowSwarmSubsystem::TakeBiteSlot(const AHollowWhispererController& Whisperer, APawn* Prey)
{
	if (FMember* Self = Find(Whisperer))
	{
		Self->BitePrey = Prey;
	}
}

void UHollowSwarmSubsystem::ReleaseBiteSlot(const AHollowWhispererController& Whisperer)
{
	if (FMember* Self = Find(Whisperer))
	{
		Self->BitePrey.Reset();
	}
}

void UHollowSwarmSubsystem::Prune()
{
	Members.RemoveAll([](const FMember& Member) { return !Member.Brain.IsValid() || !Member.Body.IsValid(); });
	Groups.RemoveAll([](const TPair<TWeakObjectPtr<APawn>, int32>& Pair) { return !Pair.Key.IsValid(); });
}

void UHollowSwarmSubsystem::UpdateTimer()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	FTimerManager& Timers = World->GetTimerManager();
	if (Members.IsEmpty())
	{
		Timers.ClearTimer(LightTimer);
	}
	else if (!Timers.IsTimerActive(LightTimer) && Config && Config->Swarm.LightCheckSeconds > 0.f)
	{
		Timers.SetTimer(LightTimer, this, &ThisClass::CheckLight, Config->Swarm.LightCheckSeconds, /*bLoop=*/true);
	}
}

bool UHollowSwarmSubsystem::HasLight(const APawn* Pawn)
{
	const IHollowLightSource* Light = Cast<IHollowLightSource>(Pawn);
	if (!Light)
	{
		return false;
	}
	TArray<FHollowLightCone> Cones;
	Light->GetLightCones(Cones);
	return !Cones.IsEmpty();
}

bool UHollowSwarmSubsystem::FindLightOn(const UWorld& World, const FVector& Point, const AActor* Ignored,
	const TConstArrayView<TPair<FHollowLightCone, const AActor*>> Cones, FHollowLightCone& OutCone)
{
	for (const TPair<FHollowLightCone, const AActor*>& Cone : Cones)
	{
		if (!Cone.Key.Contains(Point))
		{
			continue;
		}
		FCollisionQueryParams Params(SCENE_QUERY_STAT(HollowSwarmLight), /*bTraceComplex=*/false, Cone.Value);
		Params.AddIgnoredActor(Ignored);
		if (!World.LineTraceTestByChannel(Cone.Key.Origin, Point, ECC_Visibility, Params))
		{
			OutCone = Cone.Key;
			return true;
		}
	}
	return false;
}

void UHollowSwarmSubsystem::CheckLight()
{
	using namespace HollowSwarmPrivate;
	const UWorld* World = GetWorld();
	Prune();
	if (!World)
	{
		return;
	}
	++Checks;
	TArray<TPair<FHollowLightCone, const AActor*>> Cones;
	IHollowLightSource::GatherCones(World, Cones);
	int32 Lit = 0;
	int32 Frenzied = 0;
	for (const FMember& Member : Members)
	{
		AHollowWhispererController* Brain = Member.Brain.Get();
		AHollowEnemyCharacter* Body = Cast<AHollowEnemyCharacter>(Member.Body.Get());
		const UHollowWhispererDefinition* Definition = Brain ? Brain->GetWhispererDefinition() : nullptr;
		UAbilitySystemComponent* Abilities = Body ? Body->GetAbilitySystemComponent() : nullptr;
		if (!Definition || !Abilities || Body->IsDead() || Body->IsInVent())
		{
			continue;
		}
		FHollowLightCone Light;
		const bool bLit = FindLightOn(*World, Body->GetActorLocation(), Body, Cones, Light);
		if (bLit)
		{
			Brain->NoteLight(Light);   // before the effect: its tag makes the brain flinch away from this light
			Renew(*Abilities, Definition->LitEffect);
			++Lit;
		}
		const APawn* Prey = Brain->GetTarget();
		if (HollowSwarmRules::ShouldFrenzy(bLit, Brain->IsExposed(), Prey != nullptr, HasLight(Prey)))
		{
			Renew(*Abilities, Definition->FrenzyEffect);
			++Frenzied;
		}
	}
	UE_CLOG(Lit != LastLit || Frenzied != LastFrenzied, LogHollowAI, Log, TEXT("Swarm: %d of %d Whisperers lit, %d in a frenzy (%d lights)"),
		Lit, Members.Num(), Frenzied, Cones.Num());
	LastLit = Lit;
	LastFrenzied = Frenzied;
}

FString UHollowSwarmSubsystem::Describe() const
{
	// The living (the dead stay registered until their bodies go).
	TArray<int32> GroupIds;
	int32 Living = 0;
	int32 Biting = 0;
	for (const FMember& Member : Members)
	{
		const AHollowEnemyCharacter* Body = Cast<AHollowEnemyCharacter>(Member.Body.Get());
		if (Body && !Body->IsDead())
		{
			++Living;
			GroupIds.AddUnique(Member.Group);
			Biting += Member.BitePrey.IsValid() ? 1 : 0;
		}
	}
	return FString::Printf(TEXT("%d Whisperers alive in %d group(s), %d lit, %d in a frenzy, %d biting, %d light checks"), Living, GroupIds.Num(),
		LastLit, LastFrenzied, Biting, Checks);
}
