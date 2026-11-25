#include "Weapons/HollowHandsComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Camera/CameraComponent.h"
#include "Character/HollowExoFrameCharacter.h"
#include "Data/HollowWeaponDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Pawn.h"
#include "HollowGameLog.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Tags/HollowGameplayTags.h"
#include "Tools/HollowToolMountComponent.h"
#include "Weapons/HollowGA_WeaponFire.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowHandsComponent)

namespace HollowHandsPrivate
{
	/** A shot is allowed this much before the weapon's interval (timer and frame rounding on the owner). */
	constexpr double PaceSlackSeconds = 0.002;
}

UHollowHandsComponent::UHollowHandsComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

UHollowHandsComponent* UHollowHandsComponent::Get(const AActor* Owner)
{
	return Owner ? Owner->FindComponentByClass<UHollowHandsComponent>() : nullptr;
}

void UHollowHandsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams ModeParams;
	ModeParams.bIsPushBased = true;
	ModeParams.Condition = COND_SkipOwner;   // the owner predicts its own hands
	DOREPLIFETIME_WITH_PARAMS_FAST(UHollowHandsComponent, Mode, ModeParams);
	FDoRepLifetimeParams WeaponParams;
	WeaponParams.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UHollowHandsComponent, Weapon, WeaponParams);
}

void UHollowHandsComponent::BeginPlay()
{
	Super::BeginPlay();
	Sync();
}

void UHollowHandsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAbilitySystemComponent* Tagged = TaggedAbilitySystem.Get(); Tagged && bTagged)
	{
		Tagged->SetLooseGameplayTagCount(HollowTags::State_Hands_Weapon, 0);
	}
	TaggedAbilitySystem.Reset();
	bTagged = false;
	if (WeaponAssets.IsValid())
	{
		WeaponAssets->CancelHandle();
		WeaponAssets.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

EHollowHandsMode UHollowHandsComponent::Resolve(const EHollowHandsRequest Request) const
{
	EHollowHandsMode Wanted = Mode;
	switch (Request)
	{
	case EHollowHandsRequest::Tools: Wanted = EHollowHandsMode::Tools; break;
	case EHollowHandsRequest::Weapon: Wanted = EHollowHandsMode::Weapon; break;
	case EHollowHandsRequest::Cycle: Wanted = Mode == EHollowHandsMode::Tools ? EHollowHandsMode::Weapon : EHollowHandsMode::Tools; break;
	}
	return Wanted == EHollowHandsMode::Weapon && !Weapon ? EHollowHandsMode::Tools : Wanted;
}

bool UHollowHandsComponent::SetMode(const EHollowHandsMode NewMode)
{
	if (NewMode == Mode || (NewMode == EHollowHandsMode::Weapon && !Weapon))
	{
		return false;
	}
	Mode = NewMode;
	if (GetOwner()->HasAuthority())
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UHollowHandsComponent, Mode, this);
	}
	UE_LOG(LogHollowGame, Log, TEXT("%s: %s in hand"), *GetNameSafe(GetOwner()), Mode == EHollowHandsMode::Weapon ? *GetNameSafe(Weapon) : TEXT("the tools"));
	Sync();
	OnHandsChanged.Broadcast(Mode);
	return true;
}

void UHollowHandsComponent::RefreshWeapon()
{
	const UHollowWeaponDefinition* Found = nullptr;
	if (UAbilitySystemComponent* AbilitySystem = GetAbilitySystem())
	{
		for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
		{
			if (const UHollowGA_WeaponFire* Fire = Cast<UHollowGA_WeaponFire>(Spec.Ability); Fire && Fire->GetWeaponDefinition())
			{
				Found = Fire->GetWeaponDefinition();
				break;   // one weapon slot (Hollow.Item.Slot.Weapon, limit 1)
			}
		}
	}
	SetWeapon(Found);
}

void UHollowHandsComponent::SetWeapon(const UHollowWeaponDefinition* InWeapon)
{
	if (!GetOwner()->HasAuthority() || Weapon == InWeapon)
	{
		return;
	}
	Weapon = InWeapon;
	MARK_PROPERTY_DIRTY_FROM_NAME(UHollowHandsComponent, Weapon, this);
	UE_LOG(LogHollowGame, Log, TEXT("%s: carries %s"), *GetNameSafe(GetOwner()), *GetNameSafe(Weapon));
	LoadWeaponAssets();
	if (!Weapon && Mode == EHollowHandsMode::Weapon)
	{
		SetMode(EHollowHandsMode::Tools);   // the owner follows from the replicated weapon (OnRep_Weapon)
		return;
	}
	Sync();
	OnHandsChanged.Broadcast(Mode);
}

void UHollowHandsComponent::OnRep_Mode()
{
	Sync();
	OnHandsChanged.Broadcast(Mode);
}

void UHollowHandsComponent::OnRep_Weapon()
{
	LoadWeaponAssets();
	if (!Weapon && Mode == EHollowHandsMode::Weapon)
	{
		Mode = EHollowHandsMode::Tools;   // the server took the weapon away; the owner never receives that mode
	}
	Sync();
	OnHandsChanged.Broadcast(Mode);
}

void UHollowHandsComponent::LoadWeaponAssets()
{
	if (WeaponAssets.IsValid())
	{
		WeaponAssets->CancelHandle();
		WeaponAssets.Reset();
	}
	if (!Weapon || GetNetMode() == NM_DedicatedServer || !UAssetManager::IsInitialized())
	{
		return;
	}
	TArray<FSoftObjectPath> Paths;
	for (const TSoftObjectPtr<UFXSystemAsset>* System : {&Weapon->MuzzleFlash, &Weapon->Tracer, &Weapon->Impact})
	{
		if (!System->IsNull())
		{
			Paths.Add(System->ToSoftObjectPath());
		}
	}
	if (!Paths.IsEmpty())
	{
		WeaponAssets = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths);
	}
}

UAbilitySystemComponent* UHollowHandsComponent::GetAbilitySystem() const
{
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
}

void UHollowHandsComponent::Sync()
{
	// The tag where the abilities look for it: the same on the server and the owner, whoever switched.
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystem();
	const bool bWanted = IsWeaponInHand() && AbilitySystem != nullptr;
	if (UAbilitySystemComponent* Tagged = TaggedAbilitySystem.Get(); Tagged && bTagged && (Tagged != AbilitySystem || !bWanted))
	{
		Tagged->SetLooseGameplayTagCount(HollowTags::State_Hands_Weapon, 0);
		bTagged = false;
	}
	if (bWanted && !bTagged)
	{
		AbilitySystem->SetLooseGameplayTagCount(HollowTags::State_Hands_Weapon, 1);
		TaggedAbilitySystem = AbilitySystem;
		bTagged = true;
	}
	if (UHollowToolMountComponent* Mount = UHollowToolMountComponent::Get(GetOwner()))
	{
		Mount->SetHeldWeapon(IsWeaponInHand() ? Weapon.Get() : nullptr);
	}
}

void UHollowHandsComponent::GetAimView(const APawn& Shooter, FVector& OutOrigin, FVector& OutAim)
{
	OutAim = Shooter.GetViewRotation().Vector();   // the control rotation (the server's from the owner's moves), never the camera's kick
	const AHollowExoFrameCharacter* ExoFrame = Cast<AHollowExoFrameCharacter>(&Shooter);
	if (const UCameraComponent* Camera = ExoFrame ? ExoFrame->GetFirstPersonCamera() : nullptr)
	{
		OutOrigin = Camera->GetComponentLocation();
		return;
	}
	FRotator Unused;
	Shooter.GetActorEyesViewPoint(OutOrigin, Unused);
}

bool UHollowHandsComponent::CanFireAt(const double Now) const
{
	return Weapon && Now - LastShotTime >= Weapon->FireIntervalSeconds - HollowHandsPrivate::PaceSlackSeconds;
}

uint16 UHollowHandsComponent::NoteShot(const double Now)
{
	LastShotTime = Now;
	return ++ShotSerial;
}

FString UHollowHandsComponent::Describe() const
{
	return FString::Printf(TEXT("%s: %s in hand, weapon %s, shot %u, server hits %d taken / %d refused, markers %d (last %s)"), *GetNameSafe(GetOwner()),
		Mode == EHollowHandsMode::Weapon ? TEXT("weapon") : TEXT("tools"), *GetNameSafe(Weapon), ShotSerial, AcceptedHits, RejectedHits, HitMarkers,
		LexToString(LastHitMarker));
}

void UHollowHandsComponent::ShowHitMarker(const EHollowHitKind Kind)
{
	if (Kind == EHollowHitKind::None)
	{
		return;
	}
	LastHitMarker = Kind;
	++HitMarkers;
	OnHitMarker.Broadcast(Kind);
}
