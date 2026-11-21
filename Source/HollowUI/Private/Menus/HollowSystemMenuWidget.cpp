#include "Menus/HollowSystemMenuWidget.h"

#include "CommonButtonBase.h"
#include "Engine/GameInstance.h"
#include "Foundation/HollowButton.h"
#include "GameFramework/PlayerController.h"
#include "HollowUILog.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Menus/HollowMenuRules.h"
#include "Raid/HollowRaidLauncher.h"
#include "Session/HollowSessionSubsystem.h"
#include "Voice/HollowVoiceComponent.h"
#include "Voice/HollowVoiceRules.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowSystemMenuWidget)

#define LOCTEXT_NAMESPACE "HollowSystemMenu"

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
	if (InviteButton)
	{
		InviteButton->OnClicked().AddUObject(this, &ThisClass::Invite);
	}
	if (StartRaidButton)
	{
		StartRaidButton->OnClicked().AddUObject(this, &ThisClass::StartRaid);
	}
	if (LeaveButton)
	{
		LeaveButton->OnClicked().AddUObject(this, &ThisClass::Leave);
	}
	if (VoiceButton)
	{
		VoiceButton->OnClicked().AddUObject(this, &ThisClass::CycleVoice);
	}
}

void UHollowSystemMenuWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	ApplyContext(HollowMenuRules::ReadContext(GetWorld()));
	RefreshVoiceLabel();
}

void UHollowSystemMenuWidget::ApplyContext(const FHollowMenuContext& Context)
{
	const FHollowSystemMenuOptions Options = HollowMenuRules::SystemMenuOptions(Context);
	auto Show = [](UCommonButtonBase* Button, const bool bShown)
	{
		if (Button)
		{
			Button->SetVisibility(bShown ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
	};
	Show(InviteButton, Options.bInvite);
	Show(StartRaidButton, Options.bStartRaid);
	Show(LeaveButton, Options.bLeave);
}

UWidget* UHollowSystemMenuWidget::NativeGetDesiredFocusTarget() const
{
	return ResumeButton;
}

void UHollowSystemMenuWidget::Resume()
{
	DeactivateWidget();
}

void UHollowSystemMenuWidget::Invite()
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UHollowSessionSubsystem* Session = GameInstance ? GameInstance->GetSubsystem<UHollowSessionSubsystem>() : nullptr;
	UE_CLOG(!Session || !Session->ShowInviteUI(), LogHollowUI, Warning, TEXT("%s: the invite dialog is unavailable (Steam overlay off?)"), *GetName());
}

void UHollowSystemMenuWidget::StartRaid()
{
	UWorld* World = GetWorld();
	if (!HollowRaidLauncher::CanStartRaid(World))
	{
		UE_LOG(LogHollowUI, Warning, TEXT("%s: a raid starts from the hub, by the host."), *GetName());
		return;
	}
	DeactivateWidget();
	HollowRaidLauncher::Travel(World);
}

void UHollowSystemMenuWidget::Leave()
{
	const UGameInstance* GameInstance = GetGameInstance();
	if (UHollowSessionSubsystem* Session = GameInstance ? GameInstance->GetSubsystem<UHollowSessionSubsystem>() : nullptr)
	{
		DeactivateWidget();
		Session->LeaveSession();   // ends it for everyone when hosting; back to the main menu
	}
}

void UHollowSystemMenuWidget::Quit()
{
	UE_LOG(LogHollowUI, Log, TEXT("%s: quit requested"), *GetName());
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, /*bIgnorePlatformRestrictions=*/false);
}

FText UHollowSystemMenuWidget::VoiceLabel(const EHollowVoiceMode Mode)
{
	switch (Mode)
	{
	case EHollowVoiceMode::OpenMic: return LOCTEXT("VoiceOpenMic", "Voice: Open mic");
	case EHollowVoiceMode::Muted:   return LOCTEXT("VoiceMuted", "Voice: Muted");
	default:                        return LOCTEXT("VoicePushToTalk", "Voice: Push-to-talk");
	}
}

void UHollowSystemMenuWidget::CycleVoice()
{
	if (UHollowVoiceComponent* Voice = UHollowVoiceComponent::Get(GetOwningPlayer()))
	{
		Voice->SetMode(HollowVoiceRules::NextMode(Voice->GetMode()));
	}
	RefreshVoiceLabel();
}

void UHollowSystemMenuWidget::RefreshVoiceLabel()
{
	UHollowButton* Button = Cast<UHollowButton>(VoiceButton);
	const UHollowVoiceComponent* Voice = UHollowVoiceComponent::Get(GetOwningPlayer());
	if (Button)
	{
		Button->SetVisibility(Voice ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (Voice)
		{
			Button->SetLabel(VoiceLabel(Voice->GetMode()));
		}
	}
}

#undef LOCTEXT_NAMESPACE
