#include "Weapons/HollowGA_WeaponReload.h"

#include "Abilities/HollowItemCostSource.h"
#include "AbilitySystemComponent.h"
#include "Attributes/HollowWeaponSet.h"
#include "Audio/HollowAudioSubsystem.h"
#include "Data/HollowWeaponDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "HollowGameLog.h"
#include "Tags/HollowGameplayTags.h"
#include "TimerManager.h"
#include "Weapons/HollowHandsComponent.h"
#include "Weapons/HollowWeaponMath.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowGA_WeaponReload)

namespace HollowWeaponReloadPrivate
{
	/** The ammunition item (a tiny primary data asset, loaded with the raid's items already). */
	const UObject* AmmoObject(const UHollowWeaponDefinition& Weapon)
	{
		return UAssetManager::IsInitialized() && Weapon.AmmoItem.IsValid() ? UAssetManager::Get().GetPrimaryAssetPath(Weapon.AmmoItem).TryLoad() : nullptr;
	}
}

UHollowGA_WeaponReload::UHollowGA_WeaponReload(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EHollowAbilityActivationPolicy::OnInputTriggered;
	SetAssetTags(FGameplayTagContainer(HollowTags::Ability_Weapon_Reload));
	ActivationOwnedTags.AddTag(HollowTags::State_Weapon_Reloading);
	ActivationRequiredTags.AddTag(HollowTags::State_Hands_Weapon);
	ActivationBlockedTags.AddTag(HollowTags::State_Dead);
	ActivationBlockedTags.AddTag(HollowTags::State_Hands_Swapping);
	CancelAbilitiesWithTag.AddTag(HollowTags::Ability_Weapon_Aim);
}

int32 UHollowGA_WeaponReload::GetCarriedRounds(const UHollowWeaponDefinition& Weapon, const AActor* Owner)
{
	const IHollowItemCostSource* Source = IHollowItemCostSource::Find(Owner);
	const UObject* Ammo = HollowWeaponReloadPrivate::AmmoObject(Weapon);
	return Source && Ammo ? Source->GetItemCount(Ammo) : 0;
}

int32 UHollowGA_WeaponReload::GetRoundsToLoad(const UHollowWeaponDefinition& Weapon, const FGameplayAbilityActorInfo& ActorInfo)
{
	const UAbilitySystemComponent* AbilitySystem = ActorInfo.AbilitySystemComponent.Get();
	if (!AbilitySystem)
	{
		return 0;
	}
	const int32 Magazine = FMath::FloorToInt32(AbilitySystem->GetNumericAttribute(UHollowWeaponSet::GetMagazineAttribute()));
	const int32 Size = FMath::FloorToInt32(AbilitySystem->GetNumericAttribute(UHollowWeaponSet::GetMaxMagazineAttribute()));
	return HollowWeaponMath::RoundsToLoad(Magazine, Size, GetCarriedRounds(Weapon, ActorInfo.OwnerActor.Get()));
}

bool UHollowGA_WeaponReload::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	const UHollowHandsComponent* Hands = UHollowHandsComponent::Get(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	if (!WeaponDefinition || !LoadEffect || !Hands || Hands->GetWeapon() != WeaponDefinition || GetRoundsToLoad(*WeaponDefinition, *ActorInfo) <= 0)
	{
		return false;
	}
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UHollowGA_WeaponReload::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const int32 Rounds = WeaponDefinition ? GetRoundsToLoad(*WeaponDefinition, *ActorInfo) : 0;
	if (Rounds <= 0 || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}
	if (HasAuthority(&ActivationInfo))
	{
		IHollowItemCostSource* Source = IHollowItemCostSource::Find(ActorInfo->OwnerActor.Get());
		const UObject* Ammo = HollowWeaponReloadPrivate::AmmoObject(*WeaponDefinition);
		if (!Source || !Ammo || !Source->ConsumeItem(Ammo, Rounds))
		{
			UE_LOG(LogHollowGame, Warning, TEXT("%s: %d rounds of %s are not there"), *GetNameSafe(GetAvatarActorFromActorInfo()), Rounds,
				*WeaponDefinition->AmmoItem.ToString());
			EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
			return;
		}
	}
	// In the prediction window: the owner's magazine fills at once, the server's effect replaces it.
	const FGameplayEffectSpecHandle Load = MakeOutgoingGameplayEffectSpec(LoadEffect, GetAbilityLevel());
	Load.Data->SetSetByCallerMagnitude(HollowTags::Data_AmmoLoaded, static_cast<float>(Rounds));
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Load);
	if (ActorInfo->IsLocallyControlled())
	{
		if (UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(GetAvatarActorFromActorInfo()))
		{
			Audio->PlayAttached(HollowTags::Sound_Weapon_Reload, GetAvatarActorFromActorInfo()->GetRootComponent());
		}
	}
	UE_LOG(LogHollowGame, Log, TEXT("%s: reloads %d rounds into %s"), *GetNameSafe(GetAvatarActorFromActorInfo()), Rounds, *GetNameSafe(WeaponDefinition));
	GetWorld()->GetTimerManager().SetTimer(ReloadTimer, this, &ThisClass::FinishReload, FMath::Max(WeaponDefinition->ReloadSeconds, UE_KINDA_SMALL_NUMBER),
		/*bLoop=*/false);
}

void UHollowGA_WeaponReload::FinishReload()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
	}
}

void UHollowGA_WeaponReload::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const bool bReplicateEndAbility, const bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadTimer);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
