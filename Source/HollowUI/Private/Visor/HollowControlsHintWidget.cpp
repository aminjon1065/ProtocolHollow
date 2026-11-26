#include "Visor/HollowControlsHintWidget.h"

#include "Classes/HollowClassDefinition.h"
#include "Components/TextBlock.h"
#include "Foundation/HollowUISettings.h"
#include "InputAction.h"
#include "Localization/HollowLanguage.h"
#include "Settings/HollowGameUserSettings.h"
#include "ViewModels/HollowObjectiveModel.h"
#include "ViewModels/HollowVisorContext.h"
#include "Visor/HollowControlsHintSet.h"
#include "Visor/HollowVisorStyle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowControlsHintWidget)

void UHollowControlsHintWidget::SetRowsForTests(const UHollowControlsHintSet* InRows)
{
	Rows = InRows;
	Refresh();
}

void UHollowControlsHintWidget::BindContext(UHollowVisorContext& InContext)
{
	if (!Rows)
	{
		Rows = GetDefault<UHollowUISettings>()->ControlsHint.LoadSynchronous();
	}
	InputTypeHandle = InContext.OnInputTypeChanged.AddWeakLambda(this, [this](ECommonInputType) { Refresh(); });
	KeysHandle = InContext.OnKeysChanged.AddUObject(this, &ThisClass::Refresh);
	ClassHandle = InContext.OnClassChanged.AddUObject(this, &ThisClass::Refresh);
	LanguageHandle = HollowLanguage::OnApplied().AddUObject(this, &ThisClass::Refresh);
	Objective = InContext.GetObjective();
	if (UHollowObjectiveModel* Model = Objective.Get())
	{
		ObjectiveHandle = Model->OnObjectiveChanged.AddUObject(this, &ThisClass::Refresh);   // hub or raid rows
	}
	if (UHollowGameUserSettings* Settings = UHollowGameUserSettings::Get())
	{
		SettingsHandle = Settings->OnControlsHintChanged.AddUObject(this, &ThisClass::Refresh);
		ModeHandle = Settings->OnWaypointModeChanged.AddUObject(this, &ThisClass::Refresh);
	}
	Refresh();
}

void UHollowControlsHintWidget::UnbindContext(UHollowVisorContext& InContext)
{
	InContext.OnInputTypeChanged.Remove(InputTypeHandle);
	InContext.OnKeysChanged.Remove(KeysHandle);
	InContext.OnClassChanged.Remove(ClassHandle);
	HollowLanguage::OnApplied().Remove(LanguageHandle);
	if (UHollowObjectiveModel* Model = Objective.Get())
	{
		Model->OnObjectiveChanged.Remove(ObjectiveHandle);
	}
	Objective.Reset();
	if (UHollowGameUserSettings* Settings = UHollowGameUserSettings::Get())
	{
		Settings->OnControlsHintChanged.Remove(SettingsHandle);
		Settings->OnWaypointModeChanged.Remove(ModeHandle);
	}
}

bool UHollowControlsHintWidget::IsUnfolded() const
{
	const UHollowGameUserSettings* Settings = UHollowGameUserSettings::Get();
	return !Settings || Settings->IsControlsHintShown();
}

FText UHollowControlsHintWidget::GetTitleText() const
{
	const UHollowVisorContext* Owner = GetContext();
	const UHollowVisorStyle* Style = Owner ? Owner->GetStyle() : nullptr;
	if (!Style)
	{
		return FText::GetEmpty();
	}
	const FText Key = Owner->GetKeyLabel(GetDefault<UHollowUISettings>()->ControlsHintAction.LoadSynchronous());
	if (Key.IsEmpty())
	{
		return Style->ControlsTitle;
	}
	// POLISH-01: with immersive waypoints in a raid the key shows them too, and the title says so.
	const UHollowGameUserSettings* Settings = UHollowGameUserSettings::Get();
	const UHollowObjectiveModel* Model = Objective.Get();
	const bool bImmersive = Settings && Settings->GetWaypointMode() == EHollowWaypointMode::Immersive && Model && Model->GetFacts().bInRaid;
	return FText::Format(bImmersive && !Style->ControlsTitleImmersiveFormat.IsEmpty() ? Style->ControlsTitleImmersiveFormat : Style->ControlsTitleFormat, Key);
}

FText UHollowControlsHintWidget::GetListText() const
{
	const UHollowVisorContext* Owner = GetContext();
	if (!Owner || !Rows || !IsUnfolded())
	{
		return FText::GetEmpty();
	}
	const UHollowClassDefinition* Class = Owner->GetClass();
	const FGameplayTag ClassTag = Class ? Class->ClassTag : FGameplayTag();
	const UHollowObjectiveModel* Model = Objective.Get();
	const bool bInRaid = Model && Model->GetFacts().bInRaid;
	TArray<TPair<FText, FText>> Lines;
	for (const FHollowControlsHintRow& Row : Rows->Rows)
	{
		const FText& Label = UHollowControlsHintSet::LabelFor(Row, ClassTag);
		const FText Key = Owner->GetKeyLabel(Row.Action.LoadSynchronous());
		if ((Row.bRaidOnly && !bInRaid) || Label.IsEmpty() || Key.IsEmpty())
		{
			continue;   // not here, not for this class, or not mapped on this device
		}
		Lines.Emplace(Key, Label);
	}
	return HollowControlsHintRules::Lines(Lines);
}

void UHollowControlsHintWidget::Refresh()
{
	const UHollowVisorContext* Owner = GetContext();
	const FText Title = GetTitleText();
	SetVisibility(Title.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	TitleText->SetText(Title);
	const FText List = GetListText();
	ListText->SetText(List);
	ListText->SetVisibility(List.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (Owner)
	{
		TitleText->SetColorAndOpacity(FSlateColor(Owner->GetTelemetryColor()));
	}
}

FString UHollowControlsHintWidget::Describe() const
{
	return FString::Printf(TEXT("controls %s \"%s\" | %s"), IsUnfolded() ? TEXT("unfolded") : TEXT("folded"), *GetTitleText().ToString(),
		*GetListText().ToString().Replace(TEXT("\n"), TEXT(" | ")));
}
