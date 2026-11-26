#include "Menus/HollowMainMenuWidget.h"

#include "CommonButtonBase.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Foundation/HollowHUD.h"
#include "GeneralProjectSettings.h"
#include "HollowUILog.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Localization/HollowLanguage.h"
#include "Menus/HollowMenuRules.h"
#include "Session/HollowSessionSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowMainMenuWidget)

#define LOCTEXT_NAMESPACE "HollowMainMenu"

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
	if (LanguageButton)
	{
		LanguageButton->OnClicked().AddUObject(this, &ThisClass::CycleLanguage);
	}
	if (SettingsButton)
	{
		SettingsButton->OnClicked().AddUObject(this, &ThisClass::OpenSettings);
	}
	if (VersionText)
	{
		VersionText->SetText(VersionLabel());
	}
}

UWidget* UHollowMainMenuWidget::NativeGetDesiredFocusTarget() const
{
	return HostButton;   // co-op is the game's own mode (S4-06): the gamepad starts there
}

FText UHollowMainMenuWidget::VersionLabel()
{
	return FText::Format(LOCTEXT("Version", "v{0}"), FText::AsCultureInvariant(GetDefault<UGeneralProjectSettings>()->ProjectVersion));
}

void UHollowMainMenuWidget::OpenSettings()
{
	AHollowHUD* HUD = AHollowHUD::Get(GetOwningPlayer());
	UE_CLOG(!HUD || !HUD->OpenSettings(), LogHollowUI, Warning, TEXT("%s: the settings screen did not open"), *GetName());
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

void UHollowMainMenuWidget::CycleLanguage()
{
	const EHollowLanguage Language = HollowMenuRules::CycleLanguage();
	UE_LOG(LogHollowUI, Log, TEXT("%s: language %s"), *GetName(), LexToString(Language));
}

#undef LOCTEXT_NAMESPACE
