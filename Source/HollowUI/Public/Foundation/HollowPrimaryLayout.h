#pragma once

#include "CommonUserWidget.h"
#include "GameplayTagContainer.h"

#include "HollowPrimaryLayout.generated.h"

class UCommonActivatableWidget;
class UCommonActivatableWidgetContainerBase;
class UCommonActivatableWidgetStack;

/**
 * Root widget of a local player (ADR-0013): one activatable stack per layer, from the visor up to modals. The WBP
 * (W_PrimaryLayout) only arranges the stacks; screens are pushed by layer tag.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowPrimaryLayout : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** Hollow.UI.Layer.* -> its stack, or null. */
	UCommonActivatableWidgetContainerBase* GetLayer(const FGameplayTag& LayerTag) const;

	/** Creates WidgetClass on top of the layer and activates it. */
	UCommonActivatableWidget* PushWidgetToLayer(const FGameplayTag& LayerTag, TSubclassOf<UCommonActivatableWidget> WidgetClass);

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetStack> GameLayer;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetStack> GameMenuLayer;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetStack> MenuLayer;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetStack> ModalLayer;

private:
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UCommonActivatableWidgetContainerBase>> Layers;
};
