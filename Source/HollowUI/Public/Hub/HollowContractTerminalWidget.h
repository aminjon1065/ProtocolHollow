#pragma once

#include "Hub/HollowHubScreenWidget.h"

#include "HollowContractTerminalWidget.generated.h"

/**
 * The contracts terminal's screen (W_ContractTerminal, S5-04): the host picks the squad's contract (the server
 * validates it, ADR-0032) and launches the raid, the same launch as the system menu's Start raid; a guest sees the
 * host's choice change live and cannot launch. Every contract shows its sector and what it pays this player (ADR-0041).
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowContractTerminalWidget : public UHollowHubScreenWidget
{
	GENERATED_BODY()

public:
	UCommonButtonBase* GetLaunchButton() const { return LaunchButton; }

	/** Who decides, and the squad's contract with its sector and reward. */
	FText GetRoleText() const;

	/** Whether the screen's player picks and launches (the host's machine). */
	bool IsHostScreen() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual TArray<UPanelWidget*> GetLists() const override;
	virtual void RefreshRows(const FHollowProfileState& State, const FHollowEconomyTerms& Terms) override;
	virtual TArray<FPrimaryAssetId> GetNamedIds(const FHollowProfileState& State, const FHollowEconomyTerms& Terms) const override;
	virtual void RunCommand(const FHollowHubAction& Action, const FText& RowName) override;
	virtual void BindHub() override;
	virtual void UnbindHub() override;
	/** The host's Launch (the contract is chosen already), else the base's choice. */
	virtual UWidget* GetDefaultFocusTarget() const override;
	virtual TArray<UWidget*> GetOtherButtons() const override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> RowList;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> LaunchButton;

	/** Who decides here. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> RoleText;

private:
	void Launch();
	static TArray<FPrimaryAssetId> ListContracts();

	FDelegateHandle ContractHandle;
};
