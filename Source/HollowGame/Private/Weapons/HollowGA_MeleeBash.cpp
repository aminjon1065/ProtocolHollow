#include "Weapons/HollowGA_MeleeBash.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/HollowExoFrameCharacter.h"
#include "Collision/HollowCollisionChannels.h"
#include "Damage/HollowDamageSettings.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "Feel/HollowFeelSubsystem.h"
#include "HollowGameLog.h"
#include "Interfaces/HollowBreakableProp.h"
#include "Interfaces/HollowImpulseReceiver.h"
#include "Noise/HollowNoiseSubsystem.h"
#include "Perception/AISense_Damage.h"
#include "Tags/HollowGameplayTags.h"
#include "Weapons/HollowHandsComponent.h"
#include "Weapons/HollowHitMarker.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowGA_MeleeBash)

UHollowGA_MeleeBash::UHollowGA_MeleeBash(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EHollowAbilityActivationPolicy::OnInputTriggered;
	SetAssetTags(FGameplayTagContainer(HollowTags::Ability_Melee));
	ActivationBlockedTags.AddTag(HollowTags::State_Dead);
	ActivationBlockedTags.AddTag(HollowTags::State_Hands_Swapping);
	ActivationBlockedTags.AddTag(HollowTags::State_Weapon_Reloading);
	CancelAbilitiesWithTag.AddTag(HollowTags::Ability_Tool);   // the beam stops for the blow
	CancelAbilitiesWithTag.AddTag(HollowTags::Ability_Weapon_Aim);
}

void UHollowGA_MeleeBash::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const APawn* Pawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!Pawn || !CommitAbility(Handle, ActorInfo, ActivationInfo))   // the cooldown, predicted
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}
	FVector Origin;
	FVector Aim;
	UHollowHandsComponent::GetAimView(*Pawn, Origin, Aim);
	if (ActorInfo->IsLocallyControlled())
	{
		// GP-01c: the striker feels its blow and sees what it met, at once (the server's strike decides the rest).
		if (UHollowFeelSubsystem* Feel = UHollowFeelSubsystem::Get(Pawn))
		{
			Feel->EmitFor(FeelTag, Pawn);
		}
		FHitResult Met;
		if (UHollowHandsComponent* Hands = UHollowHandsComponent::Get(Pawn); Hands && Sweep(*Pawn, Origin, Aim, Met))
		{
			Hands->ShowHitMarker(HollowHitMarker::KindOf(Met.GetActor(), DamageEffect, Damage, 0.f, 0.f));
		}
	}
	if (HasAuthority(&ActivationInfo))
	{
		Strike(Origin, Aim);
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}

bool UHollowGA_MeleeBash::Sweep(const APawn& Pawn, const FVector& Origin, const FVector& Aim, FHitResult& OutHit) const
{
	const UWorld* World = Pawn.GetWorld();
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(HollowMeleeBash), /*bTraceComplex=*/false, &Pawn);
	return World && World->SweepSingleByChannel(OutHit, Origin, Origin + Aim.GetSafeNormal() * Range, FQuat::Identity, HollowCollision::Weapon,
		FCollisionShape::MakeSphere(Radius), Params) && OutHit.GetActor();
}

AActor* UHollowGA_MeleeBash::Strike(const FVector& Origin, const FVector& Aim)
{
	APawn* Pawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	if (!Pawn || !AbilitySystem || !Pawn->HasAuthority())
	{
		return nullptr;
	}
	UWorld* World = Pawn->GetWorld();
	FHitResult Hit;
	const FVector Direction = Aim.GetSafeNormal();
	if (!Sweep(*Pawn, Origin, Aim, Hit))
	{
		return nullptr;
	}
	AActor* Target = Hit.GetActor();
	if (UHollowNoiseSubsystem* Noise = UHollowNoiseSubsystem::Get(Pawn); Noise && NoiseTag.IsValid())
	{
		Noise->ReportNoise(FHollowNoiseEvent(NoiseTag, Hit.ImpactPoint, Pawn));
	}
	if (Cast<AHollowExoFrameCharacter>(Target) && !GetDefault<UHollowDamageSettings>()->bFriendlyFire)
	{
		return Target;   // a squad mate: struck, harmless
	}
	if (UAbilitySystemComponent* TargetAbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target); TargetAbilitySystem && DamageEffect)
	{
		const FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(DamageEffect, GetAbilityLevel());
		Spec.Data->SetSetByCallerMagnitude(HollowTags::Data_Damage, Damage);
		AbilitySystem->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetAbilitySystem);
		UAISense_Damage::ReportDamageEvent(World, Target, Pawn, Damage, Pawn->GetActorLocation(), Hit.ImpactPoint);
	}
	if (IHollowBreakableProp* Prop = Cast<IHollowBreakableProp>(Target))
	{
		FHollowStructuralHit PropHit;   // GP-01b: a loot crate breaks under blows
		PropHit.Amount = Damage;
		PropHit.Location = Hit.ImpactPoint;
		PropHit.Direction = Direction;
		PropHit.DamageType = HollowTags::Damage_Kinetic;
		PropHit.Instigator = Pawn;
		Prop->ApplyPropHit(PropHit);
	}
	if (IHollowImpulseReceiver* Receiver = Cast<IHollowImpulseReceiver>(Target))
	{
		FHollowImpulse Push;
		Push.Impulse = Direction * Impulse;
		Push.Location = Hit.ImpactPoint;
		Push.Instigator = Pawn;
		Receiver->ReceiveImpulse(Push);
	}
	UE_LOG(LogHollowGame, Log, TEXT("%s strikes %s"), *Pawn->GetName(), *Target->GetName());
	return Target;
}
