#include "Menus/HollowSystemMenuWidget.h"

#include "CommonButtonBase.h"
#include "HollowUILog.h"
#include "Kismet/KismetSystemLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowSystemMenuWidget)

UHollowSystemMenuWidget::UHollowSystemMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InputMode = EHollowWidgetInputMode::Menu;
	bIsBackHandler = true;   // Back deactivates the menu (CommonUI default handling)
}

void UHollowSystemMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	ResumeButton->OnClicked().AddUObject(this, &ThisClass::Resume);
	QuitButton->OnClicked().AddUObject(this, &ThisClass::Quit);
}

UWidget* UHollowSystemMenuWidget::NativeGetDesiredFocusTarget() const
{
	return ResumeButton;
}

void UHollowSystemMenuWidget::Resume()
{
	DeactivateWidget();
}

void UHollowSystemMenuWidget::Quit()
{
	UE_LOG(LogHollowUI, Log, TEXT("%s: quit requested"), *GetName());
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, /*bIgnorePlatformRestrictions=*/false);
}
