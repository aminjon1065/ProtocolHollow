#include "Foundation/HollowPrimaryLayout.h"

#include "CommonActivatableWidget.h"
#include "HollowUILog.h"
#include "HollowUITags.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowPrimaryLayout)

void UHollowPrimaryLayout::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	Layers.Add(HollowUITags::UI_Layer_Game, GameLayer);
	Layers.Add(HollowUITags::UI_Layer_GameMenu, GameMenuLayer);
	Layers.Add(HollowUITags::UI_Layer_Menu, MenuLayer);
	Layers.Add(HollowUITags::UI_Layer_Modal, ModalLayer);
}

UCommonActivatableWidgetContainerBase* UHollowPrimaryLayout::GetLayer(const FGameplayTag& LayerTag) const
{
	const TObjectPtr<UCommonActivatableWidgetContainerBase>* Layer = Layers.Find(LayerTag);
	return Layer ? Layer->Get() : nullptr;
}

UCommonActivatableWidget* UHollowPrimaryLayout::PushWidgetToLayer(const FGameplayTag& LayerTag, const TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
	UCommonActivatableWidgetContainerBase* Layer = GetLayer(LayerTag);
	if (!ensureMsgf(Layer && WidgetClass, TEXT("%s: no layer %s or no widget class"), *GetName(), *LayerTag.ToString()))
	{
		return nullptr;
	}
	UCommonActivatableWidget* Widget = Layer->AddWidget<UCommonActivatableWidget>(WidgetClass);
	UE_LOG(LogHollowUI, Log, TEXT("%s pushed on %s"), *GetNameSafe(Widget), *LayerTag.ToString());
	return Widget;
}
