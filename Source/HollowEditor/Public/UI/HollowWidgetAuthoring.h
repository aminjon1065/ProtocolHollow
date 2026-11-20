#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Layout/Margin.h"
#include "Types/SlateEnums.h"

#include "HollowWidgetAuthoring.generated.h"

class UWidget;
class UWidgetBlueprint;

/**
 * Editor-only widget tree authoring for the content scripts (ADR-0010, ADR-0013): editor Python cannot reach a
 * WBP's WidgetTree, so Build/Python/create_ui_assets.py composes the greybox layouts through these calls.
 */
UCLASS()
class HOLLOWEDITOR_API UHollowWidgetAuthoring : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Adds a widget called Name under the panel Parent, or as the root when Parent is None. Returns the template to configure. */
	UFUNCTION(BlueprintCallable, Category = "Hollow|Editor|Widgets")
	static UWidget* AddWidget(UWidgetBlueprint* Blueprint, TSubclassOf<UWidget> WidgetClass, FName Name, FName Parent);

	UFUNCTION(BlueprintCallable, Category = "Hollow|Editor|Widgets")
	static UWidget* FindWidget(UWidgetBlueprint* Blueprint, FName Name);

	/** Like AddWidget into the panel Parent, at Index among its children (an existing tree gains a widget in its place). */
	UFUNCTION(BlueprintCallable, Category = "Hollow|Editor|Widgets")
	static UWidget* InsertWidget(UWidgetBlueprint* Blueprint, TSubclassOf<UWidget> WidgetClass, FName Name, FName Parent, int32 Index);

	/** Canvas panel slot: one anchor point, alignment (pivot), position and size (or the content's size). */
	UFUNCTION(BlueprintCallable, Category = "Hollow|Editor|Widgets")
	static bool SetCanvasSlot(UWidget* Widget, FVector2D Anchor, FVector2D Alignment, FVector2D Position, FVector2D Size, bool bAutoSize);

	/** Overlay or box slot: alignment and padding; in boxes Fill > 0 fills with that weight, 0 sizes to content. */
	UFUNCTION(BlueprintCallable, Category = "Hollow|Editor|Widgets")
	static bool SetBoxSlot(UWidget* Widget, EHorizontalAlignment HorizontalAlignment, EVerticalAlignment VerticalAlignment, FMargin Padding, float Fill);

	/** Compiles the blueprint; false on errors. */
	UFUNCTION(BlueprintCallable, Category = "Hollow|Editor|Widgets")
	static bool Compile(UWidgetBlueprint* Blueprint);
};
