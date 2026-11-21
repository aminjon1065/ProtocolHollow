#include "Inventory/HollowInventoryComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/AssetManager.h"
#include "GameFramework/Actor.h"
#include "HollowEconomyLog.h"
#include "Items/HollowItemDefinition.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowInventoryComponent)

void FHollowInventoryList::PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters)
{
	if (Owner)
	{
		Owner->HandleReplicatedChange();
	}
}

UHollowInventoryComponent::UHollowInventoryComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UHollowInventoryComponent::PostInitProperties()
{
	Super::PostInitProperties();
	Inventory.Owner = this;   // after the properties were copied from the archetype (a blueprint player state's template)
}

UHollowInventoryComponent* UHollowInventoryComponent::Get(const AActor* Owner)
{
	return Owner ? Owner->FindComponentByClass<UHollowInventoryComponent>() : nullptr;
}

void UHollowInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.Condition = COND_OwnerOnly;   // nobody else needs to know what a player carries
	DOREPLIFETIME_WITH_PARAMS_FAST(UHollowInventoryComponent, Inventory, Params);
}

void UHollowInventoryComponent::ResetFromLoadout(const TConstArrayView<FHollowItemStack> Accepted)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	Clear();
	for (const FHollowItemStack& Stack : Accepted)
	{
		UHollowItemDefinition* Item = UAssetManager::IsInitialized()
			? Cast<UHollowItemDefinition>(UAssetManager::Get().GetPrimaryAssetPath(Stack.Item).TryLoad()) : nullptr;
		if (!Item)
		{
			UE_LOG(LogHollowEconomy, Warning, TEXT("%s: unknown item %s left out of the inventory"), *GetNameSafe(GetOwner()), *Stack.Item.ToString());
			continue;
		}
		AddItem(Item, Stack.Count);
	}
}

void UHollowInventoryComponent::AddItem(UHollowItemDefinition* Item, const int32 Count)
{
	if (!Item || Count <= 0 || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	FHollowInventoryEntry* Entry = FindEntry(Item);
	if (!Entry)
	{
		Entry = &Inventory.Entries.AddDefaulted_GetRef();
		Entry->Item = Item;
		Grant(Item);
	}
	Entry->Count += Count;
	Entry->Taken += Count;
	Changed(Entry);
}

void UHollowInventoryComponent::Clear()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	for (TPair<FPrimaryAssetId, FHollowAbilitySetHandles>& Pair : Granted)
	{
		Pair.Value.TakeFromAbilitySystem(AbilitySystem);
	}
	Granted.Reset();
	if (Inventory.Entries.IsEmpty())
	{
		return;
	}
	Inventory.Entries.Reset();
	Inventory.MarkArrayDirty();
	Changed(nullptr);
}

int32 UHollowInventoryComponent::GetCount(const UHollowItemDefinition* Item) const
{
	const FHollowInventoryEntry* Entry = FindEntry(Item);
	return Entry ? Entry->Count : 0;
}

int32 UHollowInventoryComponent::GetCount(const FPrimaryAssetId& Item) const
{
	const FHollowInventoryEntry* Entry = Inventory.Entries.FindByPredicate([&Item](const FHollowInventoryEntry& E)
	{
		return E.Item && E.Item->GetPrimaryAssetId() == Item;
	});
	return Entry ? Entry->Count : 0;
}

TArray<FHollowItemStack> UHollowInventoryComponent::GetSpent() const
{
	TArray<FHollowItemStack> Spent;
	for (const FHollowInventoryEntry& Entry : Inventory.Entries)
	{
		if (Entry.Item && Entry.Taken > Entry.Count)
		{
			FHollowItemStack& Stack = Spent.AddDefaulted_GetRef();
			Stack.Item = Entry.Item->GetPrimaryAssetId();
			Stack.Count = Entry.Taken - Entry.Count;
		}
	}
	return Spent;
}

FString UHollowInventoryComponent::Describe() const
{
	TArray<FString> Parts;
	for (const FHollowInventoryEntry& Entry : Inventory.Entries)
	{
		Parts.Add(FString::Printf(TEXT("%s %d/%d"), Entry.Item ? *Entry.Item->GetName() : TEXT("(unmapped)"), Entry.Count, Entry.Taken));
	}
	return Parts.IsEmpty() ? FString(TEXT("empty")) : FString::Join(Parts, TEXT(", "));
}

int32 UHollowInventoryComponent::GetItemCount(const UObject* Item) const
{
	const FHollowInventoryEntry* Entry = FindEntry(Item);
	return Entry ? Entry->Count : 0;
}

bool UHollowInventoryComponent::ConsumeItem(const UObject* Item, const int32 Count)
{
	FHollowInventoryEntry* Entry = FindEntry(Item);
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Entry || Count <= 0 || Entry->Count < Count)
	{
		return false;
	}
	Entry->Count -= Count;
	UE_LOG(LogHollowEconomy, Log, TEXT("%s: uses %d %s, %d left"), *GetNameSafe(GetOwner()), Count, *Entry->Item->GetName(), Entry->Count);
	Changed(Entry);
	return true;
}

FHollowInventoryEntry* UHollowInventoryComponent::FindEntry(const UObject* Item)
{
	return Item ? Inventory.Entries.FindByPredicate([Item](const FHollowInventoryEntry& Entry) { return Entry.Item == Item; }) : nullptr;
}

const FHollowInventoryEntry* UHollowInventoryComponent::FindEntry(const UObject* Item) const
{
	return Item ? Inventory.Entries.FindByPredicate([Item](const FHollowInventoryEntry& Entry) { return Entry.Item == Item; }) : nullptr;
}

void UHollowInventoryComponent::Grant(UHollowItemDefinition* Item)
{
	UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	if (Item->AbilitySet.IsNull())
	{
		return;
	}
	const UHollowAbilitySet* AbilitySet = Item->AbilitySet.LoadSynchronous();
	if (!AbilitySet || !AbilitySystem)
	{
		UE_LOG(LogHollowEconomy, Error, TEXT("%s: %s grants nothing (ability set %s, ability system %s)"), *GetNameSafe(GetOwner()), *Item->GetName(),
			*Item->AbilitySet.ToString(), *GetNameSafe(AbilitySystem));
		return;
	}
	// The item is the abilities' source object: an ability that costs its item takes this one.
	AbilitySet->GiveToAbilitySystem(AbilitySystem, &Granted.FindOrAdd(Item->GetPrimaryAssetId()), Item);
}

void UHollowInventoryComponent::Changed(FHollowInventoryEntry* Entry)
{
	if (Entry)
	{
		Inventory.MarkItemDirty(*Entry);
	}
	MARK_PROPERTY_DIRTY_FROM_NAME(UHollowInventoryComponent, Inventory, this);
	GetOwner()->ForceNetUpdate();   // a laid mine shows on the owner's visor at once, not at the player state's next update
	OnInventoryChanged.Broadcast();
}

void UHollowInventoryComponent::HandleReplicatedChange()
{
	OnInventoryChanged.Broadcast();
}
