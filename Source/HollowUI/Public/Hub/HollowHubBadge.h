#pragma once

#include "CoreMinimal.h"

#include "HollowHubBadge.generated.h"

/** What a coloured badge of a hub screen tells (UI-02, ADR-0052); its colours are DA_UIStyle's. */
UENUM(BlueprintType)
enum class EHollowHubBadgeKind : uint8
{
	/** Credits and prices. */
	Price,
	/** Stash and loadout slots. */
	Slot,
	/** Insurance: an insured slot, the insured count. */
	Insured
};

/** One badge: its kind and its words ("120 cr", "Stash 3/20", "INSURED"). */
struct HOLLOWUI_API FHollowHubBadge
{
	EHollowHubBadgeKind Kind = EHollowHubBadgeKind::Price;
	FText Text;
};
