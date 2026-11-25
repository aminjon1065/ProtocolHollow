#pragma once

#include "Foundation/HollowActivatableWidget.h"
#include "Hub/HollowHubScreenRules.h"
#include "Profile/HollowProfileTypes.h"

#include "HollowHubScreenWidget.generated.h"

class AHollowHubTerminal;
class APlayerController;
class IHollowProfileSource;
class UCommonButtonBase;
class UHollowHubRowWidget;
class UPanelWidget;
class UTextBlock;

/**
 * Base of the hub's terminal screens on the Menu layer (S5-04, ADR-0033): menu input (the pawn stands still), Back
 * (Esc / B) or Close leave it and close the terminal. Event driven, no tick: the rows are rebuilt when the screen is
 * shown and whenever the profile changes. Each row's buttons are previewed by HollowHubScreenRules, so what the rules
 * would refuse is greyed out with its reason. The result of the last press, or the reason of a greyed-out button in
 * focus, shows on the message line; credits and slots on the status line.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowHubScreenWidget : public UHollowActivatableWidget
{
	GENERATED_BODY()

public:
	explicit UHollowHubScreenWidget(const FObjectInitializer& ObjectInitializer);

	/** Who uses the screen at which terminal (set before it is shown: UHollowHubScreenRouter, tests). */
	void OpenFor(APlayerController* InPlayer, AHollowHubTerminal* InTerminal);

	/** Tests: a profile in memory instead of the player's own; it must outlive the screen. */
	void SetProfileSourceForTests(IHollowProfileSource* Source);

	APlayerController* GetHubPlayer() const;
	AHollowHubTerminal* GetTerminal() const { return Terminal.Get(); }

	/** Rebuilds status and rows from the profile and the hub. */
	void Refresh();

	/** The shown rows of every list, in focus order (tests, Hollow.UI.Dump). */
	TArray<UHollowHubRowWidget*> GetRowWidgets() const;

	/** The shown row named Name, if any (tests). */
	UHollowHubRowWidget* FindRow(const FText& Name) const;

	FText GetMessage() const;
	FText GetStatus() const;
	UCommonButtonBase* GetCloseButton() const { return CloseButton; }

	/** What a row's button does; RowName names it on the message line. */
	void RunAction(const FHollowHubAction& Action, const FText& RowName);

	/** Public for tests (Back = Close: the screen goes, the terminal closes). */
	virtual bool NativeOnHandleBackAction() override;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	/** The screen's lists, in focus order. */
	virtual TArray<UPanelWidget*> GetLists() const PURE_VIRTUAL(UHollowHubScreenWidget::GetLists, return {};);

	/** Fills the screen's lists (FillList) from the profile and the hub. */
	virtual void RefreshRows(const FHollowProfileState& State, const FHollowEconomyTerms& Terms) PURE_VIRTUAL(UHollowHubScreenWidget::RefreshRows, );

	/**
	 * Where the focus goes when the screen is shown and whenever CommonUI asks again (a switch between keyboard and
	 * gamepad does): the button already in focus keeps it, else GetDefaultFocusTarget.
	 */
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	/** The first button that can do something now, else the first button at all, else Close. */
	virtual UWidget* GetDefaultFocusTarget() const;

	/** Focusable buttons of the screen besides its rows and Close (the contracts screen's Launch). */
	virtual TArray<UWidget*> GetOtherButtons() const { return {}; }

	/** Ids whose names the rows show (the profile's by default). */
	virtual TArray<FPrimaryAssetId> GetNamedIds(const FHollowProfileState& State, const FHollowEconomyTerms& Terms) const;

	/** A command that is not a profile transaction (the contracts screen). */
	virtual void RunCommand(const FHollowHubAction& Action, const FText& RowName) {}

	/** A row's button took the focus (the Codex shows its log in the reader as the focus moves). */
	virtual void OnActionFocused(const FHollowHubAction& Action, const FText& RowName) {}

	/** Shown: subscribe to what the screen shows besides the profile. Hidden: let go of it. */
	virtual void BindHub() {}
	virtual void UnbindHub() {}

	/**
	 * Shows Rows in List, reusing its row widgets (a focused button keeps its focus; a focus whose button went moves
	 * to the nearest one left, else Close).
	 */
	void FillList(UPanelWidget* List, const TArray<FHollowHubRow>& Rows);

	void SetMessage(const FText& Message);

	const FHollowHubNames& GetNames() const { return Names; }

	IHollowProfileSource* GetProfileSource() const;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> CloseButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MessageText;

	/** W_HubRow (create_hub_ui_assets.py). */
	UPROPERTY(EditDefaultsOnly, Category = "Rows")
	TSubclassOf<UHollowHubRowWidget> RowClass;

private:
	void HandleRowAction(UHollowHubRowWidget* Row, int32 ActionIndex);
	void HandleRowFocus(UHollowHubRowWidget* Row, int32 ActionIndex);
	void Close();

	TWeakObjectPtr<APlayerController> Player;
	TWeakObjectPtr<AHollowHubTerminal> Terminal;

	/** Tests only (see SetProfileSourceForTests). */
	IHollowProfileSource* TestSource = nullptr;

	/** The source subscribed to while shown (the game instance's profile outlives every screen). */
	IHollowProfileSource* BoundSource = nullptr;
	FDelegateHandle ProfileHandle;

	/** A screen without a profile source (no local profile) shows an empty one. */
	FHollowProfileState NoProfile;

	FHollowHubNames Names;
};
