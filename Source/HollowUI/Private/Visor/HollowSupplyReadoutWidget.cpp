#include "Visor/HollowSupplyReadoutWidget.h"

#include "Character/HollowExoFrameCharacter.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Inventory/HollowInventoryComponent.h"
#include "Items/HollowItemDefinition.h"
#include "Tags/HollowGameplayTags.h"
#include "ViewModels/HollowVisorContext.h"
#include "Visor/HollowVisorStyle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowSupplyReadoutWidget)

void UHollowSupplyReadoutWidget::BindContext(UHollowVisorContext& InContext)
{
	SourcesHandle = InContext.OnSourcesChanged.AddUObject(this, &ThisClass::Rebind);
	Rebind();
}

void UHollowSupplyReadoutWidget::UnbindContext(UHollowVisorContext& InContext)
{
	InContext.OnSourcesChanged.Remove(SourcesHandle);
	if (UHollowInventoryComponent* Old = Inventory.Get())
	{
		Old->OnInventoryChanged.Remove(InventoryHandle);
	}
	Inventory.Reset();
}

void UHollowSupplyReadoutWidget::Rebind()
{
	const APlayerController* Player = GetContext() ? GetContext()->GetPlayer() : nullptr;
	UHollowInventoryComponent* NewInventory = Player ? UHollowInventoryComponent::Get(Player->PlayerState) : nullptr;
	if (Inventory.Get() != NewInventory)
	{
		if (UHollowInventoryComponent* Old = Inventory.Get())
		{
			Old->OnInventoryChanged.Remove(InventoryHandle);
		}
		Inventory = NewInventory;
		if (NewInventory)
		{
			InventoryHandle = NewInventory->OnInventoryChanged.AddUObject(this, &ThisClass::Refresh);
		}
	}
	Refresh();
}

FText UHollowSupplyReadoutWidget::GetSupplyText() const
{
	const UHollowVisorStyle* Style = GetContext() ? GetContext()->GetStyle() : nullptr;
	const UHollowInventoryComponent* Held = Inventory.Get();
	if (!Style || !Held)
	{
		return FText::GetEmpty();
	}
	TArray<FText> Parts;
	for (const FHollowInventoryEntry& Entry : Held->GetEntries())
	{
		if (Entry.Item && Entry.Item->ItemType.MatchesTag(HollowTags::Item_Type_Consumable))
		{
			Parts.Add(FText::Format(Style->SupplyFormat, Entry.Item->DisplayName.ToUpper(), Entry.Count));
		}
	}
	return FText::Join(Style->SupplySeparator, Parts);
}

bool UHollowSupplyReadoutWidget::IsShown() const
{
	const AHollowExoFrameCharacter* Body = GetContext() ? GetContext()->GetCharacter() : nullptr;
	return Body && !Body->IsDead() && !GetSupplyText().IsEmpty();
}

void UHollowSupplyReadoutWidget::Refresh()
{
	SetVisibility(IsShown() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	const UHollowVisorStyle* Style = GetContext() ? GetContext()->GetStyle() : nullptr;
	SupplyText->SetText(GetSupplyText());
	SupplyText->SetColorAndOpacity(FSlateColor(Style ? Style->TelemetryColor : FLinearColor::White));
}

FString UHollowSupplyReadoutWidget::Describe() const
{
	return FString::Printf(TEXT("supplies \"%s\"%s"), *GetSupplyText().ToString(), IsShown() ? TEXT("") : TEXT(" (hidden)"));
}
