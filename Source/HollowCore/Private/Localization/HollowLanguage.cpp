#include "Localization/HollowLanguage.h"

#include "HollowCoreLog.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/TextLocalizationManager.h"
#include "CoreGlobals.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowLanguage)

namespace HollowLanguagePrivate
{
	constexpr EHollowLanguage Languages[] = {EHollowLanguage::English, EHollowLanguage::Russian};

	/** Set by the first Apply; until then the language follows the current culture. */
	TOptional<EHollowLanguage> Applied;

	FSimpleMulticastDelegate& AppliedEvent()
	{
		static FSimpleMulticastDelegate Event;
		return Event;
	}
}

TConstArrayView<EHollowLanguage> HollowLanguage::All()
{
	return HollowLanguagePrivate::Languages;
}

const TCHAR* HollowLanguage::CultureName(const EHollowLanguage Language)
{
	switch (Language)
	{
	case EHollowLanguage::Russian: return TEXT("ru");
	case EHollowLanguage::English:
	default:                       return TEXT("en");
	}
}

EHollowLanguage HollowLanguage::FromCulture(const FString& Culture)
{
	// "ru", "ru-RU", "ru_RU" (some systems answer with an underscore); "rue" (Rusyn) is not Russian.
	const bool bRussian = Culture.StartsWith(TEXT("ru"), ESearchCase::IgnoreCase)
		&& (Culture.Len() == 2 || Culture[2] == TCHAR('-') || Culture[2] == TCHAR('_'));
	return bRussian ? EHollowLanguage::Russian : EHollowLanguage::English;
}

TOptional<EHollowLanguage> HollowLanguage::Parse(const FString& Name)
{
	for (const EHollowLanguage Language : All())
	{
		if (Name.Equals(CultureName(Language), ESearchCase::IgnoreCase))
		{
			return Language;
		}
	}
	return {};
}

EHollowLanguage HollowLanguage::Next(const EHollowLanguage Language)
{
	const int32 Index = All().Find(Language);
	return All()[(Index + 1) % All().Num()];
}

bool HollowLanguage::Apply(const EHollowLanguage Language)
{
	const FString Culture = CultureName(Language);
#if WITH_EDITOR
	if (GIsEditor)
	{
		// PIE and automation: preview the game's texts in Language, the editor keeps its own (the native game culture disables the preview).
		FTextLocalizationManager::Get().EnableGameLocalizationPreview(Language == EHollowLanguage::English ? FString() : Culture);
	}
	else
#endif
	if (!FInternationalization::Get().SetCurrentLanguageAndLocale(Culture))
	{
		UE_LOG(LogHollowCore, Warning, TEXT("Language %s: culture '%s' is not available (is it staged? ADR-0048)."), LexToString(Language), *Culture);
		return false;
	}
	FTextLocalizationManager::Get().WaitForAsyncTasks();   // the texts show the new language before anyone hears of it

	const bool bChanged = GetApplied() != Language;
	HollowLanguagePrivate::Applied = Language;
	if (bChanged)
	{
		UE_LOG(LogHollowCore, Display, TEXT("Language %s (%s)"), LexToString(Language), *Culture);
		HollowLanguagePrivate::AppliedEvent().Broadcast();
	}
	return true;
}

EHollowLanguage HollowLanguage::GetApplied()
{
	if (HollowLanguagePrivate::Applied.IsSet())
	{
		return HollowLanguagePrivate::Applied.GetValue();
	}
	return FromCulture(FInternationalization::Get().GetCurrentLanguage()->GetName());
}

FSimpleMulticastDelegate& HollowLanguage::OnApplied()
{
	return HollowLanguagePrivate::AppliedEvent();
}

const TCHAR* LexToString(const EHollowLanguage Language)
{
	switch (Language)
	{
	case EHollowLanguage::Russian: return TEXT("Russian");
	case EHollowLanguage::English:
	default:                       return TEXT("English");
	}
}
