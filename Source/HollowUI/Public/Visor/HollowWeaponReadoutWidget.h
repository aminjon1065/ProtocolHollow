#pragma once

#include "GameplayTagContainer.h"
#include "ViewModels/HollowAttributeWatch.h"
#include "Visor/HollowVisorElement.h"

#include "HollowWeaponReadoutWidget.generated.h"

class UAbilitySystemComponent;
class UHollowHandsComponent;
class UHollowInventoryComponent;
class UTextBlock;

/**
 * Visor telemetry of the weapon in hand (GP-01, ADR-0044): "NAILGUN  9/12  [36]" (the magazine, its size and the
 * rounds carried), "NAILGUN  RELOADING" while it reloads. From the owner's magazine attributes, raid inventory and
 * hands, all replicated to the owner; nothing ticks. Hidden with the tools in hand or without a living body.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowWeaponReadoutWidget : public UHollowVisorElement
{
	GENERATED_BODY()

public:
	FText GetReadoutText() const;
	bool IsShown() const;

	virtual FString Describe() const override;

protected:
	virtual void BindContext(UHollowVisorContext& InContext) override;
	virtual void UnbindContext(UHollowVisorContext& InContext) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ReadoutText;

private:
	void Rebind();
	void Unbind();
	void Refresh();
	void HandleReloadingChanged(const FGameplayTag Tag, int32 Count);

	FHollowAttributeWatch Magazine;
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	TWeakObjectPtr<UHollowHandsComponent> Hands;
	TWeakObjectPtr<UHollowInventoryComponent> Inventory;
	FDelegateHandle SourcesHandle;
	FDelegateHandle ClassHandle;
	FDelegateHandle HandsHandle;
	FDelegateHandle InventoryHandle;
	FDelegateHandle ReloadingHandle;
};
