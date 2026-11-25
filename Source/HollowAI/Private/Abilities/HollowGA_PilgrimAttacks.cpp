#include "Abilities/HollowGA_PilgrimAttacks.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/HollowEnemyAttackDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HollowAILog.h"
#include "Interfaces/HollowStructural.h"
#include "Tags/HollowGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowGA_PilgrimAttacks)

UHollowGA_PilgrimBreach::UHollowGA_PilgrimBreach(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetTrigger(HollowTags::Event_Enemy_Breach, HollowTags::Ability_Enemy_Pilgrim_Breach);
}

bool UHollowGA_PilgrimBreach::CanStrike(const AActor* Target) const
{
	const IHollowStructural* Structural = Cast<IHollowStructural>(Target);
	return Structural && Structural->CanTakeStructuralDamage();
}

void UHollowGA_PilgrimBreach::Strike(AActor* Target)
{
	APawn* Attacker = GetAttacker();
	IHollowStructural* Structural = Cast<IHollowStructural>(Target);
	if (!Attacker || !Structural || !Structural->CanTakeStructuralDamage())
	{
		return;
	}
	const FVector From = Attacker->GetActorLocation();
	const FVector To = Target->GetActorLocation();
	if (FVector::Dist2D(From, To) > Attacker->GetSimpleCollisionRadius() + Attack->Reach)
	{
		UE_LOG(LogHollowAI, Log, TEXT("%s: %s out of reach, the blow misses"), *Attacker->GetName(), *Target->GetName());
		return;
	}
	// The blow lands on the obstacle's plane in front of the attacker, at its chest (bulkheads pick the zone by it).
	const FVector Right = Target->GetActorRightVector();
	FHollowStructuralHit Hit;
	Hit.Amount = Attack->StructuralDamage;
	Hit.Location = To + Right * FVector::DotProduct(From - To, Right);
	Hit.Location.Z = From.Z;
	Hit.Direction = (To - From).GetSafeNormal2D();
	Hit.DamageType = HollowTags::Damage_Structural;
	Hit.Instigator = Attacker;
	const bool bChanged = Structural->ApplyStructuralDamage(Hit);
	UE_LOG(LogHollowAI, Log, TEXT("%s: structural blow %.0f at %s%s"), *Attacker->GetName(), Hit.Amount, *Target->GetName(), bChanged ? TEXT("") : TEXT(" (no effect)"));
}

UHollowGA_PilgrimMelee::UHollowGA_PilgrimMelee(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetTrigger(HollowTags::Event_Enemy_Melee, HollowTags::Ability_Enemy_Pilgrim_Melee);
}

UHollowGA_StalkerAmbush::UHollowGA_StalkerAmbush(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetTrigger(HollowTags::Event_Enemy_Ambush, HollowTags::Ability_Enemy_Stalker_Ambush);
}

UHollowGA_WhispererBite::UHollowGA_WhispererBite(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetTrigger(HollowTags::Event_Enemy_Melee, HollowTags::Ability_Enemy_Whisperer_Bite);
}
