#include "Visor/HollowResonanceReadoutWidget.h"

#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Resonance/HollowSquadResonanceComponent.h"
#include "Util/HollowEnumUtils.h"
#include "ViewModels/HollowVisorContext.h"
#include "Visor/HollowVisorStyle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowResonanceReadoutWidget)

namespace HollowResonanceReadoutPrivate
{
	constexpr float Percent = 100.f;
}

void UHollowResonanceReadoutWidget::BindContext(UHollowVisorContext& InContext)
{
	SourcesHandle = InContext.OnSourcesChanged.AddUObject(this, &ThisClass::Rebind);
	Rebind();
}

void UHollowResonanceReadoutWidget::UnbindContext(UHollowVisorContext& InContext)
{
	InContext.OnSourcesChanged.Remove(SourcesHandle);
	TierWatch.Unbind();
	if (UHollowSquadResonanceComponent* Old = Squad.Get())
	{
		Old->OnSquadResonanceChanged.Remove(SquadHandle);
	}
	Squad.Reset();
}

void UHollowResonanceReadoutWidget::Rebind()
{
	UHollowSquadResonanceComponent* NewSquad = GetContext() ? GetContext()->GetSquadResonance() : nullptr;
	if (Squad.Get() != NewSquad)
	{
		if (UHollowSquadResonanceComponent* Old = Squad.Get())
		{
			Old->OnSquadResonanceChanged.Remove(SquadHandle);
		}
		Squad = NewSquad;
		if (NewSquad)
		{
			SquadHandle = NewSquad->OnSquadResonanceChanged.AddUObject(this, &ThisClass::HandleSquadChanged);
		}
	}
	UHollowAbilitySystemComponent* AbilitySystem = GetContext() ? GetContext()->GetAbilitySystem() : nullptr;
	TierWatch.Bind(AbilitySystem, [this](EHollowResonanceTier) { Refresh(); });
}

void UHollowResonanceReadoutWidget::HandleSquadChanged(const FHollowSquadResonanceNetState& State)
{
	Refresh();
}

void UHollowResonanceReadoutWidget::Refresh()
{
	const UHollowSquadResonanceComponent* SquadState = Squad.Get();
	Normalized = SquadState ? SquadState->GetNormalized() : 0.f;
	Percent = FMath::RoundToInt32(Normalized * HollowResonanceReadoutPrivate::Percent);
	// The tag the player carries; without a body (spectating) the squad's own tier.
	Tier = TierWatch.IsBound() ? TierWatch.GetTier() : (SquadState ? SquadState->GetTier() : EHollowResonanceTier::Low);

	const UHollowVisorStyle* Style = GetContext() ? GetContext()->GetStyle() : nullptr;
	const FSlateColor Colour(Style ? Style->GetTierLook(Tier).Color : FLinearColor::White);
	ValueText->SetText(Style ? FText::Format(Style->ResonanceFormat, Percent) : FText::AsNumber(Percent));
	ValueText->SetColorAndOpacity(Colour);
	if (TierText)
	{
		TierText->SetText(Style ? Style->GetTierLook(Tier).Label : FText::GetEmpty());
		TierText->SetColorAndOpacity(Colour);
	}
	if (Bar)
	{
		Bar->SetPercent(Normalized);
		Bar->SetFillColorAndOpacity(Colour.GetSpecifiedColor());
	}
}

FString UHollowResonanceReadoutWidget::Describe() const
{
	return FString::Printf(TEXT("resonance \"%s %s\" (%d%%, %s)"), *ValueText->GetText().ToString(), TierText ? *TierText->GetText().ToString() : TEXT(""),
		Percent, *HollowEnumToString(Tier));
}
