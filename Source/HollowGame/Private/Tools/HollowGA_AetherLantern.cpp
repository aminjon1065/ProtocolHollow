#include "Tools/HollowGA_AetherLantern.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Data/HollowLanternDefinition.h"
#include "Enemies/HollowEnemyCharacter.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "HollowGameLog.h"
#include "Interfaces/HollowLightSource.h"
#include "Tags/HollowGameplayTags.h"
#include "Threat/HollowAIPlayers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowGA_AetherLantern)

namespace HollowAetherLanternPrivate
{
	template <typename TActor>
	FString Names(const TArray<TActor*>& Actors)
	{
		return FString::JoinBy(Actors, TEXT(", "), [](const TActor* Actor) { return GetNameSafe(Actor); });
	}
}

UHollowGA_AetherLantern::UHollowGA_AetherLantern(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

const UHollowLanternDefinition* UHollowGA_AetherLantern::GetLanternDefinition() const
{
	return Cast<UHollowLanternDefinition>(ToolDefinition);
}

FGameplayTag UHollowGA_AetherLantern::GetCueTag() const
{
	return HollowTags::GameplayCue_Tool_Lantern;
}

TArray<AHollowEnemyCharacter*> UHollowGA_AetherLantern::FindLitEnemies(const UWorld& World, const APawn& Wielder, const FVector& Eyes,
	const FVector& Direction, const UHollowLanternDefinition& Lantern)
{
	const FHollowLightCone Cone = MakeCone(Eyes, Direction, Lantern);

	TArray<FOverlapResult> Overlaps;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(HollowLanternCone), /*bTraceComplex=*/false, &Wielder);
	World.OverlapMultiByObjectType(Overlaps, Eyes, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Lantern.Range), Params);
	TArray<AHollowEnemyCharacter*> Lit;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AHollowEnemyCharacter* Enemy = Cast<AHollowEnemyCharacter>(Overlap.GetActor());
		if (!Enemy || Enemy->IsDead() || Enemy->IsInVent() || Lit.Contains(Enemy) || !Cone.Contains(Enemy->GetActorLocation()))
		{
			continue;
		}
		FCollisionQueryParams Sight(SCENE_QUERY_STAT(HollowLanternSight), /*bTraceComplex=*/false, &Wielder);
		Sight.AddIgnoredActor(Enemy);
		if (!World.LineTraceTestByChannel(Eyes, Enemy->GetActorLocation(), Lantern.TraceChannel, Sight))
		{
			Lit.Add(Enemy);
		}
	}
	// Overlap order is the physics scene's: sort for logs and tests.
	Lit.Sort([](const AHollowEnemyCharacter& A, const AHollowEnemyCharacter& B) { return A.GetName() < B.GetName(); });
	return Lit;
}

FHollowLightCone UHollowGA_AetherLantern::MakeCone(const FVector& Eyes, const FVector& Direction, const UHollowLanternDefinition& Lantern)
{
	FHollowLightCone Cone;
	Cone.Origin = Eyes;
	Cone.Direction = Direction.GetSafeNormal();
	Cone.HalfAngleDegrees = Lantern.ConeHalfAngleDegrees;
	Cone.Range = Lantern.Range;
	Cone.Kind = EHollowLightKind::Lantern;
	return Cone;
}

bool UHollowGA_AetherLantern::FindBeamCone(const APawn& Wielder, FHollowLightCone& OutCone)
{
	const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Wielder);
	if (!AbilitySystem)
	{
		return false;
	}
	for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
	{
		const UHollowGA_AetherLantern* Lantern = Spec.IsActive() ? Cast<UHollowGA_AetherLantern>(Spec.GetPrimaryInstance()) : nullptr;
		const UHollowLanternDefinition* Definition = Lantern ? Lantern->GetLanternDefinition() : nullptr;
		if (Definition)
		{
			// Where the samples aim (UHollowGA_ContinuousTool): from the eyes along the view.
			FVector Eyes;
			FRotator View;
			Wielder.GetActorEyesViewPoint(Eyes, View);
			OutCone = MakeCone(Eyes, View.Vector(), *Definition);
			return true;
		}
	}
	return false;
}

TArray<APawn*> UHollowGA_AetherLantern::FindAuraPlayers(const UWorld& World, const APawn& Wielder, const UHollowLanternDefinition& Lantern)
{
	TArray<APawn*> Players;
	const double RadiusSquared = FMath::Square(static_cast<double>(Lantern.AuraRadius));
	HollowAIPlayers::ForEach(&World, [&](APawn& Player)
	{
		if (FVector::DistSquared(Player.GetActorLocation(), Wielder.GetActorLocation()) <= RadiusSquared)
		{
			Players.Add(&Player);
		}
	});
	Players.Sort([](const APawn& A, const APawn& B) { return A.GetName() < B.GetName(); });
	return Players;
}

bool UHollowGA_AetherLantern::ApplyTo(UAbilitySystemComponent* Target, const TSubclassOf<UGameplayEffect>& Effect) const
{
	UAbilitySystemComponent* Source = GetAbilitySystemComponentFromActorInfo();
	if (!Source || !Target || !Effect)
	{
		return false;
	}
	const FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(Effect, GetAbilityLevel());
	return Spec.IsValid() && Source->ApplyGameplayEffectSpecToTarget(*Spec.Data, Target).WasSuccessfullyApplied();
}

void UHollowGA_AetherLantern::ApplySample(APawn& Wielder, const FVector& Eyes, const FVector& Direction, const float DeltaSeconds)
{
	using namespace HollowAetherLanternPrivate;
	const UHollowLanternDefinition* Lantern = GetLanternDefinition();
	const UWorld* World = GetWorld();
	if (!Lantern || !World)
	{
		return;
	}

	const TArray<AHollowEnemyCharacter*> Lit = FindLitEnemies(*World, Wielder, Eyes, Direction, *Lantern);
	for (AHollowEnemyCharacter* Enemy : Lit)
	{
		ApplyTo(Enemy->GetAbilitySystemComponent(), Lantern->ExposeEffect);
	}
	const TArray<APawn*> Aura = FindAuraPlayers(*World, Wielder, *Lantern);
	for (APawn* Player : Aura)
	{
		ApplyTo(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Player), Lantern->AuraEffect);
	}

	const FString LitNames = Names(Lit);
	const FString AuraNames = Names(Aura);
	if (LitNames != LastLit || AuraNames != LastAura)
	{
		UE_LOG(LogHollowGame, Log, TEXT("%s: Aether light on %d enemies [%s], aura on %d players [%s]"), *Wielder.GetName(), Lit.Num(), *LitNames,
			Aura.Num(), *AuraNames);
		LastLit = LitNames;
		LastAura = AuraNames;
	}
}

void UHollowGA_AetherLantern::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const bool bReplicateEndAbility, const bool bWasCancelled)
{
	LastLit.Reset();
	LastAura.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
