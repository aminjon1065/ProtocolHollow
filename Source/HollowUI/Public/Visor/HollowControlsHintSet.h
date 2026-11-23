#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "HollowControlsHintSet.generated.h"

class UInputAction;

/** A class's own label for a row of the cheat sheet. */
USTRUCT(BlueprintType)
struct FHollowControlsClassLabel
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls", meta = (Categories = "Hollow.Class"))
	FGameplayTag ClassTag;

	/** Empty: the class has nothing on this key, the row is hidden. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
	FText Label;
};

/** One line of the cheat sheet: an action's key on the current device and what it does. */
USTRUCT(BlueprintType)
struct FHollowControlsHintRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
	TSoftObjectPtr<UInputAction> Action;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
	TArray<FHollowControlsClassLabel> ClassLabels;

	/** Only in a raid (tools, gadgets, refuelling): the hub has no use for them. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
	bool bRaidOnly = false;
};

/** The rows of the visor's controls cheat sheet (UX-01, ADR-0042; DA_ControlsHint, chosen in UHollowUISettings). */
UCLASS(BlueprintType)
class HOLLOWUI_API UHollowControlsHintSet : public UDataAsset
{
	GENERATED_BODY()

public:
	/** What Row says to a player of ClassTag (no class: the row's own label); empty: no row for them. */
	static const FText& LabelFor(const FHollowControlsHintRow& Row, const FGameplayTag& ClassTag);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/** Top to bottom. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
	TArray<FHollowControlsHintRow> Rows;
};

/** UX-01: the cheat sheet's text (pure, tested). */
namespace HollowControlsHintRules
{
	/** Spaces between the key column and the labels. */
	constexpr int32 ColumnGap = 2;

	/** One line per key and label, the keys padded to one column (the visor's font is monospaced). */
	HOLLOWUI_API FText Lines(TConstArrayView<TPair<FText, FText>> KeysAndLabels);
}
