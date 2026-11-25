#include "Radio/HollowRadioScript.h"

#include "Tags/HollowGameplayTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowRadioScript)

#define LOCTEXT_NAMESPACE "HollowRadioScript"

int32 UHollowRadioScript::FindCue(const FGameplayTag& Trigger, const FName Sector) const
{
	int32 Anywhere = INDEX_NONE;
	for (int32 Index = 0; Index < Cues.Num(); ++Index)
	{
		const FHollowRadioCue& Cue = Cues[Index];
		if (Cue.Trigger != Trigger)
		{
			continue;
		}
		if (!Cue.Sector.IsNone() && Cue.Sector == Sector)
		{
			return Index;
		}
		if (Cue.Sector.IsNone() && Anywhere == INDEX_NONE)
		{
			Anywhere = Index;
		}
	}
	return Anywhere;
}

const FText& UHollowRadioScript::GetSpeakerName(const EHollowRadioSpeaker Speaker) const
{
	return Speaker == EHollowRadioSpeaker::Echo ? EchoName : HandlerName;
}

float UHollowRadioScript::GetLineSeconds(const FText& Text) const
{
	const float Reading = static_cast<float>(Text.ToString().Len()) / FMath::Max(CharactersPerSecond, 1.f);
	return FMath::Clamp(Reading, MinLineSeconds, FMath::Max(MinLineSeconds, MaxLineSeconds));
}

#if WITH_EDITOR
EDataValidationResult UHollowRadioScript::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	auto Fail = [&Context, &Result](const FText& Message)
	{
		Context.AddError(Message);
		Result = EDataValidationResult::Invalid;
	};
	if (HandlerName.IsEmpty() || EchoName.IsEmpty())
	{
		Fail(LOCTEXT("NoSpeakers", "The radio needs the handler's and the echo's call signs."));
	}
	if (MaxLineSeconds < MinLineSeconds)
	{
		Fail(LOCTEXT("Pacing", "MaxLineSeconds is shorter than MinLineSeconds."));
	}
	TSet<FName> Ids;
	for (const FHollowRadioCue& Cue : Cues)
	{
		const FText Name = FText::FromName(Cue.Id);
		if (Cue.Id.IsNone())
		{
			Fail(LOCTEXT("NoId", "A radio cue has no id."));
		}
		else if (Ids.Contains(Cue.Id))
		{
			Fail(FText::Format(LOCTEXT("DuplicateId", "Radio cue {0} is there twice."), Name));
		}
		Ids.Add(Cue.Id);
		if (!Cue.Trigger.MatchesTag(HollowTags::Radio) || Cue.Trigger == HollowTags::Radio)
		{
			Fail(FText::Format(LOCTEXT("Trigger", "Radio cue {0} has no Hollow.Radio.* trigger."), Name));
		}
		if (Cue.Variants.IsEmpty())
		{
			Fail(FText::Format(LOCTEXT("NoVariant", "Radio cue {0} says nothing."), Name));
		}
		for (const FHollowRadioVariant& Variant : Cue.Variants)
		{
			if (Variant.Lines.IsEmpty() || Variant.Lines.ContainsByPredicate([](const FHollowRadioLine& Line) { return Line.Text.IsEmpty(); }))
			{
				Fail(FText::Format(LOCTEXT("EmptyLine", "Radio cue {0} has an empty variant or line."), Name));
			}
		}
	}
	for (int32 Index = 0; Index < Cues.Num(); ++Index)
	{
		if (FindCue(Cues[Index].Trigger, Cues[Index].Sector) != Index)
		{
			Fail(FText::Format(LOCTEXT("Shadowed", "Radio cue {0} is never heard: an earlier cue has its trigger and sector."), FText::FromName(Cues[Index].Id)));
		}
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
