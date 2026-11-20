#include "Menus/HollowMainMenuWidget.h"

#include "CommonButtonBase.h"
#include "Engine/GameInstance.h"
#include "HollowUILog.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Session/HollowSessionSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowMainMenuWidget)

UHollowMainMenuWidget::UHollowMainMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InputMode = EHollowWidgetInputMode::Menu;
}

void UHollowMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SoloButton->OnClicked().AddUObject(this, &ThisClass::Host, false);
	HostButton->OnClicked().AddUObject(this, &ThisClass::Host, true);
	QuitButton->OnClicked().AddUObject(this, &ThisClass::Quit);
}

UWidget* UHollowMainMenuWidget::NativeGetDesiredFocusTarget() const
{
	return HostButton;
}

void UHollowMainMenuWidget::Host(const bool bOnline)
{
	const UGameInstance* GameInstance = GetGameInstance();
	UHollowSessionSubsystem* Session = GameInstance ? GameInstance->GetSubsystem<UHollowSessionSubsystem>() : nullptr;
	const EHollowSessionPrivacy Privacy = bOnline ? EHollowSessionPrivacy::InviteOnly : EHollowSessionPrivacy::Solo;
	const bool bStarted = Session && Session->HostSession(Privacy);
	UE_LOG(LogHollowUI, Log, TEXT("%s: %s %s"), *GetName(), bOnline ? TEXT("host co-op") : TEXT("play solo"), bStarted ? TEXT("started") : TEXT("refused"));
}

void UHollowMainMenuWidget::Quit()
{
	UE_LOG(LogHollowUI, Log, TEXT("%s: quit requested"), *GetName());
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, /*bIgnorePlatformRestrictions=*/false);
}
