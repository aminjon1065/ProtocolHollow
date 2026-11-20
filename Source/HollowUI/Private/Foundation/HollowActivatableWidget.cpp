#include "Foundation/HollowActivatableWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowActivatableWidget)

TOptional<FUIInputConfig> UHollowActivatableWidget::GetDesiredInputConfig() const
{
	switch (InputMode)
	{
	case EHollowWidgetInputMode::Game:
		return FUIInputConfig(ECommonInputMode::Game, GameMouseCaptureMode);
	case EHollowWidgetInputMode::Menu:
		return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
	case EHollowWidgetInputMode::GameAndMenu:
		return FUIInputConfig(ECommonInputMode::All, EMouseCaptureMode::NoCapture);
	case EHollowWidgetInputMode::Default:
	default:
		return TOptional<FUIInputConfig>();
	}
}
