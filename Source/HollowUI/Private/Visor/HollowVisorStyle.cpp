#include "Visor/HollowVisorStyle.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowVisorStyle)

#define LOCTEXT_NAMESPACE "HollowVisorStyle"

const FHollowVisorTierLook& UHollowVisorStyle::GetTierLook(const EHollowResonanceTier Tier) const
{
	static const FHollowVisorTierLook Calm;
	const int32 Index = static_cast<int32>(Tier);
	return Tiers.IsValidIndex(Index) ? Tiers[Index] : Calm;
}

const FText& UHollowVisorStyle::GetVoiceModeLabel(const EHollowVoiceMode Mode) const
{
	switch (Mode)
	{
	case EHollowVoiceMode::PushToTalk: return PushToTalkLabel;
	case EHollowVoiceMode::OpenMic:    return OpenMicLabel;
	case EHollowVoiceMode::Muted:      return MutedLabel;
	}
	return FText::GetEmpty();
}

#if WITH_EDITOR
EDataValidationResult UHollowVisorStyle::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	const int32 TierCount = static_cast<int32>(EHollowResonanceTier::Critical) + 1;
	if (Tiers.Num() != TierCount)
	{
		Context.AddError(FText::Format(LOCTEXT("TierCount", "{0}: needs one look per Resonance tier ({1}), has {2}."), FText::FromName(GetFName()), TierCount, Tiers.Num()));
		Result = EDataValidationResult::Invalid;
	}
	for (int32 Index = 0; Index < Tiers.Num(); ++Index)
	{
		const FHollowVisorTierLook& Look = Tiers[Index];
		if (Look.Label.IsEmpty() || Look.PulseHz < 0.f || Look.PulseHz > MaxPulseHz || Look.Alert < 0.f || Look.Alert > 1.f)
		{
			Context.AddError(FText::Format(LOCTEXT("TierLook", "{0}: tier {1} needs a label, an alert of 0..1 and a pulse of 0..{2} Hz (photosensitivity)."),
				FText::FromName(GetFName()), Index, MaxPulseHz));
			Result = EDataValidationResult::Invalid;
		}
	}
	const FText* Texts[] = {&ResonanceFormat, &PushToTalkLabel, &OpenMicLabel, &MutedLabel, &TransmittingFormat, &TalkingFormat, &LampOnLabel, &LampOffLabel, &LampFormat, &SupplyFormat, &SupplySeparator};
	for (const FText* Text : Texts)
	{
		if (Text->IsEmpty())
		{
			Context.AddError(FText::Format(LOCTEXT("Text", "{0}: every telemetry text needs a value."), FText::FromName(GetFName())));
			Result = EDataValidationResult::Invalid;
			break;
		}
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
