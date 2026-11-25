#include "Weapons/HollowGA_WeaponFire.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/HollowExoFrameCharacter.h"
#include "Damage/HollowDamageSettings.h"
#include "Data/HollowWeaponDefinition.h"
#include "Engine/World.h"
#include "Feel/HollowFeelSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffect.h"
#include "HollowGameLog.h"
#include "Interfaces/HollowBreakableProp.h"
#include "Interfaces/HollowImpulseReceiver.h"
#include "Noise/HollowNoiseSubsystem.h"
#include "Perception/AISense_Damage.h"
#include "Tags/HollowGameplayTags.h"
#include "Weapons/HollowHandsComponent.h"
#include "Weapons/HollowHitMarker.h"
#include "Weapons/HollowWeaponMath.h"
#include "Weapons/HollowWeaponShotData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowGA_WeaponFire)

namespace HollowWeaponFirePrivate
{
	/** The server's line of sight ends this far short of the impact (the surface the pellet hit is not in the way). */
	constexpr float SightMarginCm = 5.f;

	/** Everything one shot did to one actor: one damage effect, one push. */
	struct FTargetSum
	{
		TWeakObjectPtr<AActor> Actor;
		float Damage = 0.f;
		FVector Impulse = FVector::ZeroVector;
		FVector Location = FVector::ZeroVector;
	};

	float HalfRoundTripSeconds(const APawn& Shooter)
	{
		const APlayerState* PlayerState = Shooter.GetPlayerState();
		return PlayerState ? PlayerState->GetPingInMilliseconds() * 0.5f / 1000.f : 0.f;
	}
}

UHollowGA_WeaponFire::UHollowGA_WeaponFire(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EHollowAbilityActivationPolicy::OnInputTriggered;
	SetAssetTags(FGameplayTagContainer(HollowTags::Ability_Weapon_Fire));
	ActivationRequiredTags.AddTag(HollowTags::State_Hands_Weapon);
	ActivationBlockedTags.AddTag(HollowTags::State_Dead);
	ActivationBlockedTags.AddTag(HollowTags::State_Hands_Swapping);
	ActivationBlockedTags.AddTag(HollowTags::State_Weapon_Reloading);
}

bool UHollowGA_WeaponFire::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const UHollowHandsComponent* Hands = UHollowHandsComponent::Get(Avatar);
	if (!WeaponDefinition || !Hands || !Hands->IsWeaponInHand() || Hands->GetWeapon() != WeaponDefinition)
	{
		return false;
	}
	// The owner keeps the weapon's exact pace; the server only its cooldown (a little shorter: packets bunch up).
	if (ActorInfo->IsLocallyControlled() && !Hands->CanFireAt(Avatar->GetWorld()->GetTimeSeconds()))
	{
		return false;
	}
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UHollowGA_WeaponFire::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	APawn* Shooter = Cast<APawn>(GetAvatarActorFromActorInfo());
	UHollowHandsComponent* Hands = UHollowHandsComponent::Get(Shooter);
	UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get();
	if (!Shooter || !Hands || !AbilitySystem || !CommitAbility(Handle, ActorInfo, ActivationInfo))   // a round and the cooldown, predicted
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}
	const UWorld* World = Shooter->GetWorld();
	const bool bAuthority = HasAuthority(&ActivationInfo);
	const bool bAiming = AbilitySystem->HasMatchingGameplayTag(HollowTags::State_Weapon_Aiming);
	FVector Origin;
	FVector Aim;
	UHollowHandsComponent::GetAimView(*Shooter, Origin, Aim);
	const uint16 Serial = Hands->NoteShot(World->GetTimeSeconds());
	ExecuteShotCue(Origin, Aim, Serial, bAiming);   // the owner: predicted; the server: for everyone else
	if (bAuthority)
	{
		ReportShotNoise(*Shooter);
	}

	if (ActorInfo->IsLocallyControlled())
	{
		TArray<FHitResult> Pellets;
		TracePellets(*World, *WeaponDefinition, Shooter, Origin, Aim, Serial, bAiming, Pellets);
		ShowShotFeel(*Shooter, *Hands, Pellets, Origin);
		FHollowWeaponShotData* Shot = new FHollowWeaponShotData();
		MakeShotData(Pellets, Origin, Aim, Serial, bAiming, *Shot);
		const FGameplayAbilityTargetDataHandle Data(Shot);   // owns the shot
		if (bAuthority)
		{
			ApplyShot(*Shot, /*bTrusted=*/true);   // the listen-server host's own shot
		}
		else
		{
			AbilitySystem->CallServerSetReplicatedTargetData(Handle, ActivationInfo.GetActivationPredictionKey(), Data, FGameplayTag(),
				AbilitySystem->ScopedPredictionKey);
		}
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
		return;
	}

	// The server of a remote owner: its hits follow (the same reliable channel, right after the activation).
	WaitingKey = ActivationInfo.GetActivationPredictionKey();
	ShotDataHandle = AbilitySystem->AbilityTargetDataSetDelegate(Handle, WaitingKey).AddUObject(this, &ThisClass::HandleShotData);
	AbilitySystem->CallReplicatedTargetDataDelegatesIfSet(Handle, WaitingKey);
}

void UHollowGA_WeaponFire::HandleShotData(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag)
{
	const FGameplayAbilityTargetData* Entry = Data.Get(0);
	if (Entry && Entry->GetScriptStruct() == FHollowWeaponShotData::StaticStruct())
	{
		ApplyShot(*static_cast<const FHollowWeaponShotData*>(Entry), /*bTrusted=*/false);
	}
	else
	{
		UE_LOG(LogHollowGame, Warning, TEXT("%s: a shot without its data"), *GetNameSafe(GetAvatarActorFromActorInfo()));
	}
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/false, /*bWasCancelled=*/false);
	}
}

void UHollowGA_WeaponFire::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const bool bReplicateEndAbility, const bool bWasCancelled)
{
	if (UAbilitySystemComponent* AbilitySystem = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr; AbilitySystem && WaitingKey.IsValidKey())
	{
		AbilitySystem->AbilityTargetDataSetDelegate(Handle, WaitingKey).Remove(ShotDataHandle);
		AbilitySystem->ConsumeClientReplicatedTargetData(Handle, WaitingKey);
	}
	ShotDataHandle.Reset();
	WaitingKey = FPredictionKey();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UHollowGA_WeaponFire::TracePellets(const UWorld& World, const UHollowWeaponDefinition& Weapon, const APawn* Shooter, const FVector& Origin,
	const FVector& Aim, const uint16 Serial, const bool bAiming, TArray<FHitResult>& OutPellets)
{
	TArray<FVector> Directions;
	HollowWeaponMath::PelletDirections(Aim, FMath::Min(Weapon.Pellets, FHollowWeaponShotData::MaxHits), Weapon.GetSpreadDegrees(bAiming), Serial, Directions);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HollowWeaponShot), /*bTraceComplex=*/false, Shooter);
	OutPellets.Reset(Directions.Num());
	for (const FVector& Direction : Directions)
	{
		FHitResult& Hit = OutPellets.AddDefaulted_GetRef();
		const FVector End = Origin + Direction * Weapon.Range;
		if (!World.LineTraceSingleByChannel(Hit, Origin, End, Weapon.TraceChannel, Params))
		{
			Hit = FHitResult(Origin, End);   // flew its range
		}
	}
}

void UHollowGA_WeaponFire::MakeShotData(const TArray<FHitResult>& Pellets, const FVector& Origin, const FVector& Aim, const uint16 Serial,
	const bool bAiming, FHollowWeaponShotData& OutShot)
{
	OutShot.Origin = Origin;
	OutShot.Aim = Aim;
	OutShot.Serial = Serial;
	OutShot.bAiming = bAiming;
	OutShot.Hits.Reset();
	for (int32 Pellet = 0; Pellet < Pellets.Num() && OutShot.Hits.Num() < FHollowWeaponShotData::MaxHits; ++Pellet)
	{
		const FHitResult& Hit = Pellets[Pellet];
		if (Hit.bBlockingHit && Hit.GetActor())
		{
			FHollowPelletHit& Sent = OutShot.Hits.AddDefaulted_GetRef();
			Sent.Actor = Hit.GetActor();
			Sent.Impact = Hit.ImpactPoint;
			Sent.Normal = Hit.ImpactNormal;
			Sent.Pellet = static_cast<uint8>(Pellet);
		}
	}
}

int32 UHollowGA_WeaponFire::ApplyShot(const FHollowWeaponShotData& Shot, const bool bTrusted)
{
	using namespace HollowWeaponFirePrivate;
	APawn* Shooter = Cast<APawn>(GetAvatarActorFromActorInfo());
	UHollowHandsComponent* Hands = UHollowHandsComponent::Get(Shooter);
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	const UHollowWeaponDefinition* Weapon = WeaponDefinition;
	if (!Shooter || !Hands || !AbilitySystem || !Weapon || !Shooter->HasAuthority())
	{
		return 0;
	}
	UWorld* World = Shooter->GetWorld();
	FVector ServerOrigin;
	FVector ServerAim;
	UHollowHandsComponent::GetAimView(*Shooter, ServerOrigin, ServerAim);
	TArray<FVector> Directions;
	HollowWeaponMath::PelletDirections(Shot.Aim, FMath::Min(Weapon->Pellets, FHollowWeaponShotData::MaxHits), Weapon->GetSpreadDegrees(Shot.bAiming),
		Shot.Serial, Directions);

	TArray<FTargetSum, TInlineAllocator<FHollowWeaponShotData::MaxHits>> Targets;
	int32 Taken = 0;
	for (const FHollowPelletHit& Hit : Shot.Hits)
	{
		AActor* Target = Hit.Actor;
		if (!IsValid(Target) || Target == Shooter || !Directions.IsValidIndex(Hit.Pellet))
		{
			Hands->CountServerHit(false);
			continue;
		}
		if (!bTrusted)
		{
			FHollowShotCheck Check;
			Check.ServerOrigin = ServerOrigin;
			Check.ServerAim = ServerAim;
			Check.ClaimedOrigin = Shot.Origin;
			Check.ClaimedDirection = Directions[Hit.Pellet];
			Check.ImpactPoint = Hit.Impact;
			Check.TargetBounds = Target->GetComponentsBoundingBox(/*bNonColliding=*/false);
			Check.TargetVelocity = Target->GetVelocity();
			Check.LatencySeconds = HalfRoundTripSeconds(*Shooter);
			Check.Range = Weapon->Range;
			Check.SpreadDegrees = Weapon->SpreadDegrees;   // the hip's: aiming only ever tightens it
			Check.AimToleranceDegrees = Weapon->AimToleranceDegrees;
			Check.OriginToleranceCm = Weapon->OriginToleranceCm;
			Check.BoundsToleranceCm = Weapon->BoundsToleranceCm;
			FCollisionQueryParams Sight(SCENE_QUERY_STAT(HollowWeaponSight), /*bTraceComplex=*/false, Shooter);
			Sight.AddIgnoredActor(Target);
			const FVector Short = Hit.Impact - (Hit.Impact - ServerOrigin).GetSafeNormal() * SightMarginCm;
			Check.bLineOfSight = !World->LineTraceTestByObjectType(ServerOrigin, Short, FCollisionObjectQueryParams(ECC_WorldStatic), Sight);
			const EHollowShotVerdict Verdict = HollowWeaponMath::CheckHit(Check);
			if (Verdict != EHollowShotVerdict::Accepted)
			{
				Hands->CountServerHit(false);
				UE_LOG(LogHollowGame, Verbose, TEXT("%s: pellet %d on %s refused (%s)"), *Shooter->GetName(), Hit.Pellet, *Target->GetName(),
					HollowWeaponMath::LexVerdict(Verdict));
				continue;
			}
		}
		Hands->CountServerHit(true);
		++Taken;
		FTargetSum* Sum = Targets.FindByPredicate([Target](const FTargetSum& Entry) { return Entry.Actor.Get() == Target; });
		if (!Sum)
		{
			Sum = &Targets.AddDefaulted_GetRef();
			Sum->Actor = Target;
			Sum->Location = Hit.Impact;
		}
		Sum->Damage += Weapon->GetPelletDamage(FVector::Dist(Shot.Origin, Hit.Impact));
		Sum->Impulse += Directions[Hit.Pellet] * Weapon->ImpulsePerPellet;
	}

	const bool bFriendlyFire = GetDefault<UHollowDamageSettings>()->bFriendlyFire;
	for (const FTargetSum& Sum : Targets)
	{
		AActor* Target = Sum.Actor.Get();
		if (!Target || (Cast<AHollowExoFrameCharacter>(Target) && !bFriendlyFire))
		{
			continue;   // a squad mate: the pellet stopped there, harmless
		}
		UAbilitySystemComponent* TargetAbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
		if (TargetAbilitySystem && Weapon->DamageEffect && Sum.Damage > 0.f)
		{
			const FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(Weapon->DamageEffect, GetAbilityLevel());
			Spec.Data->SetSetByCallerMagnitude(HollowTags::Data_Damage, Sum.Damage);
			Spec.Data->SetSetByCallerMagnitude(HollowTags::Data_ArmorPenetration, Weapon->ArmorPenetration);
			Spec.Data->SetSetByCallerMagnitude(HollowTags::Data_ExposedPenetration, Weapon->ExposedPenetrationBonus);
			Spec.Data->GetContext().AddOrigin(Shot.Origin);
			AbilitySystem->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetAbilitySystem);
			UAISense_Damage::ReportDamageEvent(World, Target, Shooter, Sum.Damage, Shooter->GetActorLocation(), Sum.Location);
		}
		if (IHollowBreakableProp* Prop = Cast<IHollowBreakableProp>(Target))
		{
			// GP-01b: a loot crate takes the pellets' damage as it is (no armour); doors and bulkheads only a breach opens.
			FHollowStructuralHit Hit;
			Hit.Amount = Sum.Damage;
			Hit.Location = Sum.Location;
			Hit.Direction = Sum.Impulse.GetSafeNormal();
			Hit.DamageType = HollowTags::Damage_Kinetic;
			Hit.Instigator = Shooter;
			Prop->ApplyPropHit(Hit);
		}
		if (IHollowImpulseReceiver* Receiver = Cast<IHollowImpulseReceiver>(Target); Receiver && !Sum.Impulse.IsNearlyZero())
		{
			FHollowImpulse Impulse;
			Impulse.Impulse = Sum.Impulse;
			Impulse.Location = Sum.Location;
			Impulse.Instigator = Shooter;
			Receiver->ReceiveImpulse(Impulse);
		}
		UE_LOG(LogHollowGame, Verbose, TEXT("%s: %s hits %s for %.1f"), *Shooter->GetName(), *Weapon->WeaponTag.ToString(), *Target->GetName(), Sum.Damage);
	}
	return Taken;
}

void UHollowGA_WeaponFire::ExecuteShotCue(const FVector& Origin, const FVector& Aim, const uint16 Serial, const bool bAiming)
{
	FGameplayCueParameters Cue;
	Cue.SourceObject = WeaponDefinition;
	Cue.Instigator = GetAvatarActorFromActorInfo();
	Cue.EffectCauser = GetAvatarActorFromActorInfo();
	Cue.Location = Origin;
	Cue.Normal = Aim;
	Cue.RawMagnitude = Serial;
	Cue.NormalizedMagnitude = bAiming ? 1.f : 0.f;
	K2_ExecuteGameplayCueWithParams(HollowTags::GameplayCue_Weapon_Fire, Cue);
}

void UHollowGA_WeaponFire::ShowShotFeel(const APawn& Shooter, UHollowHandsComponent& Hands, const TArray<FHitResult>& Pellets, const FVector& Origin) const
{
	const UHollowWeaponDefinition* Weapon = WeaponDefinition;
	if (!Weapon)
	{
		return;
	}
	if (UHollowFeelSubsystem* Feel = UHollowFeelSubsystem::Get(&Shooter))
	{
		Feel->EmitFor(Weapon->FeelTag, &Shooter);
	}
	// What each target takes, as the server sums it (one marker a shot: the strongest).
	TArray<TPair<const AActor*, float>, TInlineAllocator<FHollowWeaponShotData::MaxHits>> Sums;
	for (const FHitResult& Pellet : Pellets)
	{
		const AActor* Target = Pellet.GetActor();
		if (!Target)
		{
			continue;
		}
		TPair<const AActor*, float>* Sum = Sums.FindByPredicate([Target](const TPair<const AActor*, float>& Entry) { return Entry.Key == Target; });
		if (!Sum)
		{
			Sum = &Sums.Emplace_GetRef(Target, 0.f);
		}
		Sum->Value += Weapon->GetPelletDamage(FVector::Dist(Origin, Pellet.ImpactPoint));
	}
	EHollowHitKind Kind = EHollowHitKind::None;
	for (const TPair<const AActor*, float>& Sum : Sums)
	{
		Kind = HollowHitMarker::Strongest(Kind, HollowHitMarker::KindOf(Sum.Key, Weapon->DamageEffect, Sum.Value, Weapon->ArmorPenetration,
			Weapon->ExposedPenetrationBonus));
	}
	Hands.ShowHitMarker(Kind);
}

void UHollowGA_WeaponFire::ReportShotNoise(APawn& Shooter) const
{
	if (UHollowNoiseSubsystem* Noise = UHollowNoiseSubsystem::Get(&Shooter); Noise && WeaponDefinition->NoiseTag.IsValid())
	{
		Noise->ReportNoise(FHollowNoiseEvent(WeaponDefinition->NoiseTag, Shooter.GetActorLocation(), &Shooter));
	}
}
