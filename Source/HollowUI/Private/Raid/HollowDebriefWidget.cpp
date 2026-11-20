#include "Raid/HollowDebriefWidget.h"

#include "CommonButtonBase.h"
#include "Components/TextBlock.h"
#include "Engine/AssetManager.h"
#include "Items/HollowItemDefinition.h"
#include "Profile/HollowProfileSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowDebriefWidget)

#define LOCTEXT_NAMESPACE "HollowDebrief"

UHollowDebriefWidget::UHollowDebriefWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InputMode = EHollowWidgetInputMode::Menu;
}

void UHollowDebriefWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	CloseButton->OnClicked().AddUObject(this, &ThisClass::Close);
}

UWidget* UHollowDebriefWidget::NativeGetDesiredFocusTarget() const
{
	return CloseButton;
}

void UHollowDebriefWidget::SetReport(const FHollowRaidReport& InReport)
{
	Report = InReport;
	TitleText->SetText(GetTitle());
	DetailsText->SetText(GetDetails());
}

FText UHollowDebriefWidget::GetTitle() const
{
	return OutcomeTitle(Report.Result.Outcome);
}

FText UHollowDebriefWidget::GetDetails() const
{
	const FHollowRaidResult& Result = Report.Result;
	const int32 Seconds = FMath::RoundToInt(Result.RaidSeconds);
	FFormatNamedArguments Args;
	Args.Add(TEXT("Aboard"), Result.SquadExtracted);
	Args.Add(TEXT("Squad"), Result.SquadSize);
	Args.Add(TEXT("Minutes"), Seconds / 60);
	FNumberFormattingOptions TwoDigits;
	TwoDigits.SetMinimumIntegralDigits(2);
	Args.Add(TEXT("Seconds"), FText::AsNumber(Seconds % 60, &TwoDigits));
	Args.Add(TEXT("Sold"), DescribeItems(Report.Delta.Sold));
	Args.Add(TEXT("Earned"), Report.Delta.Credits);
	Args.Add(TEXT("Before"), Report.CreditsBefore);
	Args.Add(TEXT("After"), Report.CreditsAfter);
	Args.Add(TEXT("Kept"), DescribeItems(Report.Delta.Kept));
	Args.Add(TEXT("Lost"), DescribeItems(Report.Delta.Lost));
	return FText::Format(LOCTEXT("Details",
		"Aboard the shuttle: {Aboard} of {Squad}    Raid time: {Minutes}:{Seconds}\n"
		"Delivered: {Sold} (+{Earned} cr)\n"
		"Credits: {Before} -> {After}\n"
		"Kept: {Kept}\n"
		"Lost: {Lost}"), Args);
}

FText UHollowDebriefWidget::OutcomeTitle(const EHollowRaidOutcome Outcome)
{
	switch (Outcome)
	{
	case EHollowRaidOutcome::Extracted:
		return LOCTEXT("Extracted", "EXTRACTED");
	case EHollowRaidOutcome::Dead:
		return LOCTEXT("Dead", "KILLED IN ACTION");
	case EHollowRaidOutcome::MissingInAction:
		return LOCTEXT("MissingInAction", "LEFT BEHIND");
	case EHollowRaidOutcome::SignalLost:
		return LOCTEXT("SignalLost", "SIGNAL LOST");
	case EHollowRaidOutcome::Abandoned:
		return LOCTEXT("Abandoned", "RAID ABANDONED");
	case EHollowRaidOutcome::None:
		break;
	}
	return LOCTEXT("None", "RAID OVER");
}

FText UHollowDebriefWidget::DescribeItems(const TArray<FHollowItemStack>& Stacks)
{
	TArray<FText> Parts;
	for (const FHollowItemStack& Stack : Stacks)
	{
		// Item definitions are tiny data assets; the debrief loads the ones it names.
		const UHollowItemDefinition* Definition = UAssetManager::IsInitialized()
			? Cast<UHollowItemDefinition>(UAssetManager::Get().GetPrimaryAssetPath(Stack.Item).TryLoad()) : nullptr;
		const FText Name = Definition ? Definition->DisplayName : FText::FromName(Stack.Item.PrimaryAssetName);
		Parts.Add(Stack.Count > 1 ? FText::Format(LOCTEXT("Stack", "{0} x{1}"), Name, Stack.Count) : Name);
	}
	return Parts.IsEmpty() ? LOCTEXT("Nothing", "nothing") : FText::Join(LOCTEXT("Separator", ", "), Parts);
}

void UHollowDebriefWidget::Close()
{
	if (UHollowProfileSubsystem* Profile = UHollowProfileSubsystem::Get(this))
	{
		Profile->AcknowledgeReport();
	}
	DeactivateWidget();
}

#undef LOCTEXT_NAMESPACE
