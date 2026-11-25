#pragma once

#include "CoreMinimal.h"

#include "HollowLanguage.generated.h"

/** The languages the game is localized in (LOC-01, ADR-0048): English is the native one, Russian is translated. */
UENUM()
enum class EHollowLanguage : uint8
{
	English,
	Russian
};

/**
 * Which language the player's texts show in (LOC-01, ADR-0048). The texts themselves are LOCTEXT in code and string
 * table entries in data; Content/Localization/Game holds their Russian translation. Applying a language never needs a
 * restart: every FText that keeps its source (a key, a table entry, a format of those) shows the new language at once,
 * and OnApplied lets the few places that compose plain strings build them again.
 */
namespace HollowLanguage
{
	/** Every language, in the order the language button cycles through them. */
	HOLLOWCORE_API TConstArrayView<EHollowLanguage> All();

	/** The culture name the localization uses for Language: "en", "ru". */
	HOLLOWCORE_API const TCHAR* CultureName(EHollowLanguage Language);

	/** The language for a culture of the system: Russian for any "ru" culture (ru, ru-RU...), English for any other one. */
	HOLLOWCORE_API EHollowLanguage FromCulture(const FString& Culture);

	/** The language a saved or typed name stands for ("en", "ru", any case); unset for anything else. */
	HOLLOWCORE_API TOptional<EHollowLanguage> Parse(const FString& Name);

	/** The next language after Language (the language button). */
	HOLLOWCORE_API EHollowLanguage Next(EHollowLanguage Language);

	/**
	 * Shows the player's texts in Language from now on. A game switches its culture (texts, numbers and plural rules); the
	 * editor (PIE, automation) previews the game's localization instead, so the editor's own interface keeps its language.
	 * Returns false if the culture could not be set (no ICU data staged for it).
	 */
	HOLLOWCORE_API bool Apply(EHollowLanguage Language);

	/** The language last applied; before any, the one of the current culture. */
	HOLLOWCORE_API EHollowLanguage GetApplied();

	/** Fires after Apply changed the language (the texts are already resolved again). */
	HOLLOWCORE_API FSimpleMulticastDelegate& OnApplied();
}

HOLLOWCORE_API const TCHAR* LexToString(EHollowLanguage Language);
