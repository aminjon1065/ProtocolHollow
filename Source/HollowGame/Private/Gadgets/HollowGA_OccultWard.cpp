#include "Gadgets/HollowGA_OccultWard.h"

#include "Engine/World.h"
#include "Gadgets/HollowGadgetPlacement.h"
#include "Gadgets/HollowOccultWard.h"
#include "Gadgets/HollowOccultWardDefinition.h"
#include "GameFramework/Character.h"
#include "HollowGameLog.h"
#include "Tags/HollowGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowGA_OccultWard)

UHollowGA_OccultWard::UHollowGA_OccultWard(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EHollowAbilityActivationPolicy::OnInputTriggered;
	ItemCostPerUse = 1;   // every ward laid is one of the wards the player brought (DA_Item_OccultWard grants this, ADR-0030)
	SetAssetTags(FGameplayTagContainer(HollowTags::Ability_Gadget_OccultWard));
	ActivationBlockedTags.AddTag(HollowTags::State_Dead);
	ActivationBlockedTags.AddTag(HollowTags::Cooldown_Gadget);   // shared with every gadget: one press never spends two
}

bool UHollowGA_OccultWard::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	const AHollowOccultWard* Defaults = WardClass ? WardClass->GetDefaultObject<AHollowOccultWard>() : nullptr;
	return Defaults && Defaults->GetDefinition() && Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr)
		&& Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UHollowGA_OccultWard::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}
	if (HasAuthority(&ActivationInfo))
	{
		PlaceWard();
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}

void UHollowGA_OccultWard::PlaceWard()
{
	ACharacter* Placer = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UWorld* World = GetWorld();
	const UHollowOccultWardDefinition* Definition = WardClass->GetDefaultObject<AHollowOccultWard>()->GetDefinition();
	if (!Placer || !World || !Definition)
	{
		return;
	}
	if (AHollowOccultWard* Previous = ActiveWard.Get())
	{
		UE_LOG(LogHollowGame, Log, TEXT("%s: takes back %s"), *Placer->GetName(), *Previous->GetName());
		Previous->Destroy();   // one ward per player
	}
	const FTransform Transform = HollowGadgetPlacement::OnFloorBelow(*Placer, Definition->PlaceTraceDistance);
	FActorSpawnParameters Spawn;
	Spawn.Owner = Placer;
	Spawn.Instigator = Placer;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ActiveWard = World->SpawnActor<AHollowOccultWard>(WardClass, Transform, Spawn);
	UE_LOG(LogHollowGame, Log, TEXT("%s: lays %s at %s"), *Placer->GetName(), *GetNameSafe(ActiveWard.Get()), *Transform.GetLocation().ToCompactString());
}
