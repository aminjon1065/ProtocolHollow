#pragma once

#include "Visor/HollowVisorElement.h"

#include "HollowSupplyReadoutWidget.generated.h"

class UHollowInventoryComponent;
class UTextBlock;

/**
 * Visor telemetry of the consumables the player carries (S5-01, ADR-0030): "PULSE MINE 2   FUEL CELL 3", from the raid
 * inventory (it replicates to its owner only). Follows the player's state and its inventory's changes, nothing ticks;
 * hidden without consumables or without a living body.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowSupplyReadoutWidget : public UHollowVisorElement
{
	GENERATED_BODY()

public:
	FText GetSupplyText() const;
	bool IsShown() const;
	virtual FString Describe() const override;

protected:
	virtual void BindContext(UHollowVisorContext& InContext) override;
	virtual void UnbindContext(UHollowVisorContext& InContext) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SupplyText;

private:
	void Rebind();
	void Refresh();

	TWeakObjectPtr<UHollowInventoryComponent> Inventory;
	FDelegateHandle SourcesHandle;
	FDelegateHandle InventoryHandle;
};
