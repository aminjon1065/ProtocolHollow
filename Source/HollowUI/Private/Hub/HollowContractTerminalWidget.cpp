#include "Hub/HollowContractTerminalWidget.h"

#include "CommonButtonBase.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/AssetManager.h"
#include "Framework/HollowPlayerController.h"
#include "HollowUILog.h"
#include "Hub/HollowContractDefinition.h"
#include "Hub/HollowHubStateComponent.h"
#include "Hub/HollowHubTerminal.h"
#include "Raid/HollowRaidLauncher.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowContractTerminalWidget)

#define LOCTEXT_NAMESPACE "HollowHubScreens"

void UHollowContractTerminalWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	LaunchButton->OnClicked().AddUObject(this, &ThisClass::Launch);
}

bool UHollowContractTerminalWidget::IsHostScreen() const
{
	return AHollowHubTerminal::IsHostSide(GetHubPlayer());
}

TArray<FPrimaryAssetId> UHollowContractTerminalWidget::ListContracts()
{
	TArray<FPrimaryAssetId> Contracts;
	if (UAssetManager::IsInitialized())
	{
		UAssetManager::Get().GetPrimaryAssetIdList(UHollowContractDefinition::AssetType, Contracts);
	}
	return Contracts;
}

UWidget* UHollowContractTerminalWidget::GetDefaultFocusTarget() const
{
	return LaunchButton->IsVisible() ? LaunchButton.Get() : Super::GetDefaultFocusTarget();
}

TArray<UWidget*> UHollowContractTerminalWidget::GetOtherButtons() const
{
	return {LaunchButton};
}

TArray<UPanelWidget*> UHollowContractTerminalWidget::GetLists() const
{
	return {RowList};
}

TArray<FPrimaryAssetId> UHollowContractTerminalWidget::GetNamedIds(const FHollowProfileState& State, const FHollowEconomyTerms& Terms) const
{
	// The contracts, and the items they pay with (the reward's names).
	TArray<FPrimaryAssetId> Ids = ListContracts();
	for (const FPrimaryAssetId& Id : ListContracts())
	{
		if (const UHollowContractDefinition* Contract = UHollowContractDefinition::Load(Id))
		{
			for (const FPrimaryAssetId& Item : Contract->RewardItems)
			{
				Ids.AddUnique(Item);
			}
		}
	}
	return Ids;
}

FText UHollowContractTerminalWidget::GetRoleText() const
{
	return RoleText->GetText();
}

void UHollowContractTerminalWidget::BindHub()
{
	if (UHollowHubStateComponent* Hub = UHollowHubStateComponent::Get(GetWorld()))
	{
		ContractHandle = Hub->OnContractChanged.AddWeakLambda(this, [this](const FPrimaryAssetId&) { Refresh(); });
	}
}

void UHollowContractTerminalWidget::UnbindHub()
{
	if (UHollowHubStateComponent* Hub = UHollowHubStateComponent::Get(GetWorld()))
	{
		Hub->OnContractChanged.Remove(ContractHandle);
	}
	ContractHandle.Reset();
}

void UHollowContractTerminalWidget::RefreshRows(const FHollowProfileState& State, const FHollowEconomyTerms& Terms)
{
	const UHollowHubStateComponent* Hub = UHollowHubStateComponent::Get(GetWorld());
	const FPrimaryAssetId Selected = Hub ? Hub->GetSelectedContract() : FPrimaryAssetId();
	const bool bHost = IsHostScreen();
	// Priced for this player: their own ship's core bonus (ADR-0041).
	const TArray<FHollowContractOffer> Offers = FHollowContractOffer::Load(ListContracts(), HollowHubScreenRules::CoreValuePercent(State, Terms));
	FillList(RowList, HollowHubScreenRules::ContractRows(Offers, Selected, bHost, GetNames()));
	LaunchButton->SetVisibility(bHost && HollowRaidLauncher::CanStartRaid(GetWorld()) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	RoleText->SetText(HollowHubScreenRules::ContractRoleText(Offers, Selected, bHost, GetNames()));
}

void UHollowContractTerminalWidget::RunCommand(const FHollowHubAction& Action, const FText& RowName)
{
	AHollowPlayerController* HubPlayer = Cast<AHollowPlayerController>(GetHubPlayer());
	if (Action.Command != EHollowHubCommand::SelectContract || !HubPlayer)
	{
		return;
	}
	UE_LOG(LogHollowUI, Log, TEXT("%s: %s chooses %s"), *GetName(), *HubPlayer->GetName(), *Action.Contract.ToString());
	HubPlayer->Server_SelectContract(Action.Contract);   // the server's answer arrives as the hub state's change
	SetMessage(FText::Format(LOCTEXT("Chose", "Contract: {0}"), RowName));
}

void UHollowContractTerminalWidget::Launch()
{
	UWorld* World = GetWorld();
	if (!HollowRaidLauncher::CanStartRaid(World))
	{
		UE_LOG(LogHollowUI, Warning, TEXT("%s: a raid starts from the hub, by the host."), *GetName());
		SetMessage(LOCTEXT("HostLaunches", "Only the host launches the raid."));
		return;
	}
	UE_LOG(LogHollowUI, Log, TEXT("%s: launch"), *GetName());
	DeactivateWidget();
	HollowRaidLauncher::Travel(World);
}

#undef LOCTEXT_NAMESPACE
