#include "Visor/HollowWeaponReadoutWidget.h"

#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Attributes/HollowWeaponSet.h"
#include "Character/HollowExoFrameCharacter.h"
#include "Components/TextBlock.h"
#include "Data/HollowWeaponDefinition.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Inventory/HollowInventoryComponent.h"
#include "Tags/HollowGameplayTags.h"
#include "ViewModels/HollowVisorContext.h"
#include "Weapons/HollowHandsComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowWeaponReadoutWidget)

#define LOCTEXT_NAMESPACE "HollowWeaponReadout"

void UHollowWeaponReadoutWidget::BindContext(UHollowVisorContext& InContext)
{
	SourcesHandle = InContext.OnSourcesChanged.AddUObject(this, &ThisClass::Rebind);
	ClassHandle = InContext.OnClassChanged.AddUObject(this, &ThisClass::Refresh);   // the class's telemetry colour
	Rebind();
}

void UHollowWeaponReadoutWidget::UnbindContext(UHollowVisorContext& InContext)
{
	InContext.OnSourcesChanged.Remove(SourcesHandle);
	InContext.OnClassChanged.Remove(ClassHandle);
	Unbind();
}

void UHollowWeaponReadoutWidget::Unbind()
{
	Magazine.Unbind();
	if (UHollowHandsComponent* Old = Hands.Get())
	{
		Old->OnHandsChanged.Remove(HandsHandle);
	}
	if (UHollowInventoryComponent* Old = Inventory.Get())
	{
		Old->OnInventoryChanged.Remove(InventoryHandle);
	}
	if (UAbilitySystemComponent* Old = AbilitySystem.Get())
	{
		Old->RegisterGameplayTagEvent(HollowTags::State_Weapon_Reloading).Remove(ReloadingHandle);
	}
	Hands.Reset();
	Inventory.Reset();
	AbilitySystem.Reset();
}

void UHollowWeaponReadoutWidget::Rebind()
{
	Unbind();
	UHollowVisorContext* Context = GetContext();
	AHollowExoFrameCharacter* Body = Context ? Context->GetCharacter() : nullptr;
	const APlayerController* Player = Context ? Context->GetPlayer() : nullptr;
	if (UHollowHandsComponent* NewHands = UHollowHandsComponent::Get(Body))
	{
		HandsHandle = NewHands->OnHandsChanged.AddWeakLambda(this, [this](EHollowHandsMode) { Refresh(); });
		Hands = NewHands;
	}
	if (UHollowInventoryComponent* NewInventory = Player ? UHollowInventoryComponent::Get(Player->PlayerState) : nullptr)
	{
		InventoryHandle = NewInventory->OnInventoryChanged.AddUObject(this, &ThisClass::Refresh);
		Inventory = NewInventory;
	}
	if (UHollowAbilitySystemComponent* NewAbilitySystem = Context ? Context->GetAbilitySystem() : nullptr)
	{
		ReloadingHandle = NewAbilitySystem->RegisterGameplayTagEvent(HollowTags::State_Weapon_Reloading).AddUObject(this, &ThisClass::HandleReloadingChanged);
		AbilitySystem = NewAbilitySystem;
		Magazine.Bind(NewAbilitySystem, UHollowWeaponSet::GetMagazineAttribute(), UHollowWeaponSet::GetMaxMagazineAttribute(),
			[this](float, float) { Refresh(); });
	}
	Refresh();
}

void UHollowWeaponReadoutWidget::HandleReloadingChanged(const FGameplayTag Tag, const int32 Count)
{
	Refresh();
}

FText UHollowWeaponReadoutWidget::GetReadoutText() const
{
	const UHollowHandsComponent* InHand = Hands.Get();
	const UHollowWeaponDefinition* Weapon = InHand && InHand->IsWeaponInHand() ? InHand->GetWeapon() : nullptr;
	if (!Weapon)
	{
		return FText::GetEmpty();
	}
	const FText Name = Weapon->DisplayName.ToUpper();
	const UAbilitySystemComponent* Abilities = AbilitySystem.Get();
	if (Abilities && Abilities->HasMatchingGameplayTag(HollowTags::State_Weapon_Reloading))
	{
		return FText::Format(LOCTEXT("Reloading", "{0}  RELOADING"), Name);
	}
	const UHollowInventoryComponent* Held = Inventory.Get();
	const int32 Carried = Held ? Held->GetCount(Weapon->AmmoItem) : 0;
	return FText::Format(LOCTEXT("Magazine", "{0}  {1}/{2}  [{3}]"), Name, FMath::FloorToInt32(Magazine.GetValue()),
		FMath::FloorToInt32(Magazine.GetMax()), Carried);
}

bool UHollowWeaponReadoutWidget::IsShown() const
{
	const AHollowExoFrameCharacter* Body = GetContext() ? GetContext()->GetCharacter() : nullptr;
	return Body && !Body->IsDead() && !GetReadoutText().IsEmpty();
}

void UHollowWeaponReadoutWidget::Refresh()
{
	SetVisibility(IsShown() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	ReadoutText->SetText(GetReadoutText());
	ReadoutText->SetColorAndOpacity(FSlateColor(GetContext() ? GetContext()->GetTelemetryColor() : FLinearColor::White));
}

FString UHollowWeaponReadoutWidget::Describe() const
{
	return FString::Printf(TEXT("weapon \"%s\"%s"), *GetReadoutText().ToString(), IsShown() ? TEXT("") : TEXT(" (hidden)"));
}

#undef LOCTEXT_NAMESPACE
