#include "Visor/HollowLoreNoticeWidget.h"

#include "Components/TextBlock.h"
#include "Crafting/HollowEconomyConfig.h"
#include "Engine/World.h"
#include "Lore/HollowLoreEntryDefinition.h"
#include "Lore/HollowLoreRules.h"
#include "Profile/HollowProfileSubsystem.h"
#include "Raid/HollowRaidStateComponent.h"
#include "TimerManager.h"
#include "ViewModels/HollowVisorContext.h"
#include "Visor/HollowVisorStyle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowLoreNoticeWidget)

void UHollowLoreNoticeWidget::BindContext(UHollowVisorContext& InContext)
{
	SourcesHandle = InContext.OnSourcesChanged.AddUObject(this, &ThisClass::BindRaid);
	BindRaid();
	SetVisibility(bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UHollowLoreNoticeWidget::UnbindContext(UHollowVisorContext& InContext)
{
	InContext.OnSourcesChanged.Remove(SourcesHandle);
	if (UHollowRaidStateComponent* Old = Raid.Get())
	{
		Old->OnLoreFound.Remove(LoreHandle);
	}
	Raid.Reset();
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimer);
	}
}

void UHollowLoreNoticeWidget::BindRaid()
{
	const UHollowVisorContext* Owner = GetContext();
	UHollowRaidStateComponent* NewRaid = UHollowRaidStateComponent::Get(Owner ? Owner->GetWorld() : nullptr);
	if (Raid.Get() == NewRaid)
	{
		return;
	}
	if (UHollowRaidStateComponent* Old = Raid.Get())
	{
		Old->OnLoreFound.Remove(LoreHandle);
	}
	Raid = NewRaid;
	if (NewRaid)
	{
		LoreHandle = NewRaid->OnLoreFound.AddUObject(this, &ThisClass::ShowLore);
	}
}

void UHollowLoreNoticeWidget::ShowLore(const FPrimaryAssetId& Entry, const bool bNew)
{
	const UHollowVisorContext* Owner = GetContext();
	const UHollowVisorStyle* Style = Owner ? Owner->GetStyle() : nullptr;
	const UHollowLoreEntryDefinition* Definition = UHollowLoreEntryDefinition::Load(Entry);
	if (!Style || !Definition)
	{
		return;
	}
	// This machine's archive after the download, of every log (no profile, a test: the raid's downloads).
	TArray<FPrimaryAssetId> All;
	for (const UHollowLoreEntryDefinition* Each : UHollowLoreEntryDefinition::LoadAll())
	{
		All.Add(Each->GetPrimaryAssetId());
	}
	const UHollowProfileSubsystem* Profile = UHollowProfileSubsystem::Get(this);
	const UHollowRaidStateComponent* RaidState = Raid.Get();
	const int32 Found = Profile ? HollowLoreRules::CountKnown(All, Profile->GetProfile().Lore)
		: (RaidState ? HollowLoreRules::CountKnown(All, RaidState->GetLoreFound()) : 1);
	const UHollowEconomyConfig* Economy = UHollowEconomyConfig::Get();
	TitleText->SetText(FText::Format(Style->LoreNoticeFormat, Found, All.Num()));
	TitleText->SetColorAndOpacity(FSlateColor(Owner->GetTelemetryColor()));
	EntryText->SetText(Definition->Title);
	EntryText->SetColorAndOpacity(FSlateColor(Style->ObjectiveColor));
	DetailText->SetText(bNew ? FText::Format(Style->LoreNoticeNew, Economy ? Economy->LoreBonusCredits : 0) : Style->LoreNoticeKnown);
	DetailText->SetColorAndOpacity(FSlateColor(Owner->GetTelemetryColor()));
	bShown = true;
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(HideTimer, this, &ThisClass::Hide, FMath::Max(Style->LoreNoticeSeconds, 0.1f), /*bLoop=*/false);
	}
}

void UHollowLoreNoticeWidget::Hide()
{
	bShown = false;
	SetVisibility(ESlateVisibility::Collapsed);
}

FText UHollowLoreNoticeWidget::GetTitleText() const
{
	return TitleText->GetText();
}

FText UHollowLoreNoticeWidget::GetEntryText() const
{
	return EntryText->GetText();
}

FText UHollowLoreNoticeWidget::GetDetailText() const
{
	return DetailText->GetText();
}

FString UHollowLoreNoticeWidget::Describe() const
{
	return bShown ? FString::Printf(TEXT("lore notice shown \"%s\" \"%s\" \"%s\""), *GetTitleText().ToString(), *GetEntryText().ToString(), *GetDetailText().ToString())
		: FString(TEXT("lore notice hidden"));
}
