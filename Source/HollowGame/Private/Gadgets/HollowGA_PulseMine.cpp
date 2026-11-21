#include "Gadgets/HollowGA_PulseMine.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Gadgets/HollowPulseMine.h"
#include "Gadgets/HollowPulseMineDefinition.h"
#include "GameFramework/Character.h"
#include "HollowGameLog.h"
#include "Tags/HollowGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowGA_PulseMine)

UHollowGA_PulseMine::UHollowGA_PulseMine(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EHollowAbilityActivationPolicy::OnInputTriggered;
	ItemCostPerUse = 1;   // every mine laid is one of the mines the player brought (DA_Item_PulseMine grants this, ADR-0030)
	SetAssetTags(FGameplayTagContainer(HollowTags::Ability_Gadget_PulseMine));
	ActivationBlockedTags.AddTag(HollowTags::State_Dead);
}

bool UHollowGA_PulseMine::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	const AHollowPulseMine* Defaults = MineClass ? MineClass->GetDefaultObject<AHollowPulseMine>() : nullptr;
	return Defaults && Defaults->GetDefinition() && Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr)
		&& Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UHollowGA_PulseMine::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}
	if (HasAuthority(&ActivationInfo))
	{
		PlaceMine();
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}

void UHollowGA_PulseMine::PlaceMine()
{
	ACharacter* Placer = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UWorld* World = GetWorld();
	const UHollowPulseMineDefinition* Definition = MineClass->GetDefaultObject<AHollowPulseMine>()->GetDefinition();
	if (!Placer || !World || !Definition)
	{
		return;
	}
	if (AHollowPulseMine* Previous = ActiveMine.Get(); Previous && !Previous->HasDetonated())
	{
		UE_LOG(LogHollowGame, Log, TEXT("%s: takes back %s"), *Placer->GetName(), *Previous->GetName());
		Previous->Destroy();   // one mine per player
	}
	const FVector Feet = Placer->GetActorLocation() - FVector(0.0, 0.0, Placer->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	FHitResult Floor;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HollowPulseMinePlace), /*bTraceComplex=*/false, Placer);
	const bool bFloor = World->LineTraceSingleByChannel(Floor, Placer->GetActorLocation(), Feet - FVector(0.0, 0.0, Definition->PlaceTraceDistance), ECC_Visibility, Params);
	const FTransform Transform(FRotator(0.0, Placer->GetActorRotation().Yaw, 0.0), bFloor ? Floor.ImpactPoint : Feet);

	FActorSpawnParameters Spawn;
	Spawn.Owner = Placer;
	Spawn.Instigator = Placer;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ActiveMine = World->SpawnActor<AHollowPulseMine>(MineClass, Transform, Spawn);
	UE_LOG(LogHollowGame, Log, TEXT("%s: lays %s at %s"), *Placer->GetName(), *GetNameSafe(ActiveMine.Get()), *Transform.GetLocation().ToCompactString());
}
