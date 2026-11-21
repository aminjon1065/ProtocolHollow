#include "Hub/HollowHubScreenWidget.h"

#include "CommonButtonBase.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Foundation/HollowButton.h"
#include "GameFramework/PlayerController.h"
#include "HollowUILog.h"
#include "Hub/HollowHubRowWidget.h"
#include "Hub/HollowHubTerminal.h"
#include "Hub/HollowHubTerminalSubsystem.h"
#include "Profile/HollowProfileSource.h"
#include "Profile/HollowProfileSubsystem.h"
#include "Util/HollowEnumUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowHubScreenWidget)

#define LOCTEXT_NAMESPACE "HollowHubScreens"

UHollowHubScreenWidget::UHollowHubScreenWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InputMode = EHollowWidgetInputMode::Menu;   // the pawn stands while a terminal is in use
	bIsBackHandler = true;
}

void UHollowHubScreenWidget::OpenFor(APlayerController* InPlayer, AHollowHubTerminal* InTerminal)
{
	Player = InPlayer;
	Terminal = InTerminal;
}

void UHollowHubScreenWidget::SetProfileSourceForTests(IHollowProfileSource* Source)
{
	TestSource = Source;
}

APlayerController* UHollowHubScreenWidget::GetHubPlayer() const
{
	return Player.IsValid() ? Player.Get() : GetOwningPlayer();
}

IHollowProfileSource* UHollowHubScreenWidget::GetProfileSource() const
{
	return TestSource ? TestSource : UHollowProfileSubsystem::Get(this);
}

void UHollowHubScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	CloseButton->OnClicked().AddUObject(this, &ThisClass::Close);
}

void UHollowHubScreenWidget::NativeOnActivated()
{
	// Rows first: CommonUI focuses the desired target once the screen is active.
	BoundSource = GetProfileSource();
	if (BoundSource)
	{
		ProfileHandle = BoundSource->ProfileChanged().AddUObject(this, &ThisClass::Refresh);
	}
	BindHub();
	SetMessage(FText::GetEmpty());
	Refresh();
	Super::NativeOnActivated();
	UE_LOG(LogHollowUI, Log, TEXT("%s: open for %s"), *GetName(), *GetNameSafe(GetHubPlayer()));
}

void UHollowHubScreenWidget::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();
	if (BoundSource)
	{
		BoundSource->ProfileChanged().Remove(ProfileHandle);
		BoundSource = nullptr;
	}
	UnbindHub();
	// Leaving the screen leaves its terminal (closed first, or another one opened since: nothing to close).
	UHollowHubTerminalSubsystem* Terminals = UHollowHubTerminalSubsystem::Get(GetWorld());
	APlayerController* HubPlayer = GetHubPlayer();
	if (Terminals && Terminal.IsValid() && Terminals->GetOpenTerminal(HubPlayer) == Terminal.Get())
	{
		Terminals->Close(HubPlayer);
	}
	UE_LOG(LogHollowUI, Log, TEXT("%s: closed"), *GetName());
}

bool UHollowHubScreenWidget::NativeOnHandleBackAction()
{
	Close();
	return true;
}

void UHollowHubScreenWidget::Close()
{
	DeactivateWidget();
}

void UHollowHubScreenWidget::Refresh()
{
	const IHollowProfileSource* Source = GetProfileSource();
	const FHollowProfileState& State = Source ? Source->GetProfileState() : NoProfile;
	const FHollowEconomyTerms Terms = FHollowEconomyTerms::Load(State);
	Names = FHollowHubNames::Load(GetNamedIds(State, Terms));
	StatusText->SetText(Source ? HollowHubScreenRules::StatusLine(State, Terms) : FText::GetEmpty());
	RefreshRows(State, Terms);
}

TArray<FPrimaryAssetId> UHollowHubScreenWidget::GetNamedIds(const FHollowProfileState& State, const FHollowEconomyTerms& Terms) const
{
	return HollowHubScreenRules::NamedIds(State, Terms);
}

void UHollowHubScreenWidget::FillList(UPanelWidget* List, const TArray<FHollowHubRow>& Rows)
{
	if (!List || !ensureMsgf(RowClass, TEXT("%s: RowClass is not set (create_hub_ui_assets.py)"), *GetName()))
	{
		return;
	}
	// Where this player's focus is in the list now.
	APlayerController* Focuser = GetHubPlayer();
	int32 FocusRow = INDEX_NONE;
	int32 FocusAction = INDEX_NONE;
	for (int32 Index = 0; Index < List->GetChildrenCount() && FocusRow == INDEX_NONE; ++Index)
	{
		const UHollowHubRowWidget* Row = Cast<UHollowHubRowWidget>(List->GetChildAt(Index));
		FocusAction = Row && Row->IsVisible() ? Row->GetFocusedAction(Focuser) : INDEX_NONE;
		FocusRow = FocusAction != INDEX_NONE ? Index : INDEX_NONE;
	}

	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		UHollowHubRowWidget* Row = Index < List->GetChildrenCount() ? Cast<UHollowHubRowWidget>(List->GetChildAt(Index)) : nullptr;
		if (!Row)
		{
			Row = CreateWidget<UHollowHubRowWidget>(this, RowClass);
			Row->OnActionClicked.AddUObject(this, &ThisClass::HandleRowAction);
			Row->OnActionFocused.AddUObject(this, &ThisClass::HandleRowFocus);
			List->AddChild(Row);
		}
		Row->SetRow(Rows[Index]);
		Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	for (int32 Index = Rows.Num(); Index < List->GetChildrenCount(); ++Index)
	{
		List->GetChildAt(Index)->SetVisibility(ESlateVisibility::Collapsed);
	}

	// A focused button that went: the nearest one left (same row, then up, then down), else Close.
	if (FocusRow == INDEX_NONE || (Rows.IsValidIndex(FocusRow) && Rows[FocusRow].Actions.IsValidIndex(FocusAction)))
	{
		return;
	}
	auto ButtonOf = [&](const int32 Index) -> UWidget*
	{
		const UHollowHubRowWidget* Row = Rows.IsValidIndex(Index) && !Rows[Index].Actions.IsEmpty() ? Cast<UHollowHubRowWidget>(List->GetChildAt(Index)) : nullptr;
		return Row ? Row->GetActionButton(FMath::Min(FocusAction, Rows[Index].Actions.Num() - 1)) : nullptr;
	};
	UWidget* Target = nullptr;
	for (int32 Index = FMath::Min(FocusRow, Rows.Num() - 1); Index >= 0 && !Target; --Index)
	{
		Target = ButtonOf(Index);
	}
	for (int32 Index = FocusRow + 1; Index < Rows.Num() && !Target; ++Index)
	{
		Target = ButtonOf(Index);
	}
	(Target ? Target : CloseButton.Get())->SetFocus();
}

UWidget* UHollowHubScreenWidget::NativeGetDesiredFocusTarget() const
{
	// CommonUI asks again when the player switches between keyboard and gamepad: the focus stays where it is, so
	// the next press acts on the button the player sees highlighted.
	APlayerController* HubPlayer = GetHubPlayer();
	auto HasFocus = [HubPlayer](const UWidget* Widget)
	{
		return Widget && (HubPlayer && HubPlayer->GetLocalPlayer() ? Widget->HasUserFocus(HubPlayer) || Widget->HasUserFocusedDescendants(HubPlayer)
			: Widget->HasAnyUserFocus() || Widget->HasFocusedDescendants());
	};
	for (const UHollowHubRowWidget* Row : GetRowWidgets())
	{
		const int32 Focused = Row->GetFocusedAction(HubPlayer);
		if (Focused != INDEX_NONE)
		{
			return Row->GetActionButton(Focused);
		}
	}
	TArray<UWidget*> Others = GetOtherButtons();
	Others.Add(CloseButton);
	for (UWidget* Other : Others)
	{
		if (HasFocus(Other))
		{
			return Other;
		}
	}
	return GetDefaultFocusTarget();
}

UWidget* UHollowHubScreenWidget::GetDefaultFocusTarget() const
{
	UWidget* FirstButton = nullptr;
	for (const UHollowHubRowWidget* Row : GetRowWidgets())
	{
		const TArray<FHollowHubAction>& Actions = Row->GetRow().Actions;
		for (int32 Index = 0; Index < Actions.Num(); ++Index)
		{
			if (Actions[Index].bAvailable)
			{
				return Row->GetActionButton(Index);
			}
			FirstButton = FirstButton ? FirstButton : Row->GetActionButton(Index);
		}
	}
	return FirstButton ? FirstButton : CloseButton.Get();
}

TArray<UHollowHubRowWidget*> UHollowHubScreenWidget::GetRowWidgets() const
{
	TArray<UHollowHubRowWidget*> Rows;
	for (const UPanelWidget* List : GetLists())
	{
		for (int32 Index = 0; List && Index < List->GetChildrenCount(); ++Index)
		{
			UHollowHubRowWidget* Row = Cast<UHollowHubRowWidget>(List->GetChildAt(Index));
			if (Row && Row->GetVisibility() != ESlateVisibility::Collapsed)
			{
				Rows.Add(Row);
			}
		}
	}
	return Rows;
}

UHollowHubRowWidget* UHollowHubScreenWidget::FindRow(const FText& Name) const
{
	for (UHollowHubRowWidget* Row : GetRowWidgets())
	{
		if (Row->GetRow().Name.EqualTo(Name))
		{
			return Row;
		}
	}
	return nullptr;
}

FText UHollowHubScreenWidget::GetMessage() const
{
	return MessageText->GetText();
}

FText UHollowHubScreenWidget::GetStatus() const
{
	return StatusText->GetText();
}

void UHollowHubScreenWidget::SetMessage(const FText& Message)
{
	MessageText->SetText(Message);
}

void UHollowHubScreenWidget::RunAction(const FHollowHubAction& Action, const FText& RowName)
{
	if (!Action.bAvailable)
	{
		SetMessage(FText::Format(LOCTEXT("Refused", "{0}: {1}"), RowName, Action.Reason));
		return;
	}
	if (Action.Command != EHollowHubCommand::Transaction)
	{
		RunCommand(Action, RowName);
		return;
	}
	IHollowProfileSource* Source = GetProfileSource();
	if (!Source)
	{
		UE_LOG(LogHollowUI, Warning, TEXT("%s: no profile on this machine"), *GetName());
		return;
	}
	const EHollowTransactionResult Result = Source->ApplyTransaction(Action.Transaction);   // applied: the rows refresh at once
	UE_LOG(LogHollowUI, Log, TEXT("%s: %s / %s -> %s"), *GetName(), *RowName.ToString(), *Action.Label.ToString(), *HollowEnumToString(Result));
	SetMessage(Result == EHollowTransactionResult::Applied ? FText::Format(LOCTEXT("Done", "{0}: {1} - done"), RowName, Action.Label)
		: FText::Format(LOCTEXT("Failed", "{0}: {1} - {2}"), RowName, Action.Label, HollowHubScreenRules::ResultText(Result)));
}

void UHollowHubScreenWidget::HandleRowAction(UHollowHubRowWidget* Row, const int32 ActionIndex)
{
	// Copies: applying refreshes the rows, this one included.
	const FHollowHubAction Action = Row->GetRow().Actions[ActionIndex];
	const FText RowName = Row->GetRow().Name;
	RunAction(Action, RowName);
}

void UHollowHubScreenWidget::HandleRowFocus(UHollowHubRowWidget* Row, const int32 ActionIndex)
{
	const FHollowHubAction& Action = Row->GetRow().Actions[ActionIndex];
	SetMessage(Action.bAvailable ? FText::GetEmpty() : FText::Format(LOCTEXT("Why", "{0}: {1}"), Action.Label, Action.Reason));
}

#undef LOCTEXT_NAMESPACE
