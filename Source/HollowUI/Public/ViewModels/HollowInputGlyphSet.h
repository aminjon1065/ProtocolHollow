#pragma once

#include "CommonInputTypeEnum.h"
#include "Engine/DataAsset.h"
#include "InputCoreTypes.h"

#include "HollowInputGlyphSet.generated.h"

USTRUCT(BlueprintType)
struct FHollowKeyGlyph
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Glyph")
	FKey Key;

	/** Short text shown in prompts ("E", "LMB", "X", "RT"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Glyph")
	FText Label;
};

/** Key labels for prompts (DA_InputGlyphs). Text until there is icon art (ADR-0013). */
UCLASS(BlueprintType)
class HOLLOWUI_API UHollowInputGlyphSet : public UDataAsset
{
	GENERATED_BODY()

public:
	/** The glyph label of Key, or the engine's short key name. */
	FText GetLabel(const FKey& Key) const;

	/** First of Keys that belongs to InputType: gamepad keys for Gamepad, the others for mouse and keyboard. */
	static FKey PickKey(TConstArrayView<FKey> Keys, ECommonInputType InputType);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Glyphs")
	TArray<FHollowKeyGlyph> Glyphs;
};
