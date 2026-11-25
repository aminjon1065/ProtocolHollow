#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Localization/HollowLanguage.h"

class UUserWidget;

/** LOC-01 (ADR-0048): helpers of the localization tests (switching the language, reading and laying out widgets). */
namespace HollowLocalizationTests
{
	/** Shows the texts in Language for the scope, English again after it (the other tests read English). */
	struct FScopedLanguage
	{
		explicit FScopedLanguage(const EHollowLanguage Language) : bApplied(HollowLanguage::Apply(Language)) {}
		~FScopedLanguage() { HollowLanguage::Apply(EHollowLanguage::English); }
		FScopedLanguage(const FScopedLanguage&) = delete;
		FScopedLanguage& operator=(const FScopedLanguage&) = delete;
		const bool bApplied;
	};

	/** Whether Text has a Cyrillic letter. */
	bool HasCyrillic(const FString& Text);

	/** Every non-empty text in Widget's tree (text blocks and Hollow buttons' labels, nested user widgets too), by path. */
	TMap<FString, FString> ReadTexts(const UUserWidget& Widget);

	/**
	 * The single-line texts of Widget laid out on a ViewSize screen that are wider than the room they get there
	 * ("path: text (desired > room)"). Wrapping texts and texts in a scale box (it shrinks them) always fit.
	 */
	TArray<FString> FindOverflows(UUserWidget& Widget, const FVector2D& ViewSize);

	/** The entries of After that are not those of Before ("path: before -> after"), for a failing comparison. */
	TArray<FString> Differences(const TMap<FString, FString>& Before, const TMap<FString, FString>& After);

	/** Whether Slate can measure text here (a renderer with its font services). */
	bool CanMeasureText();
}

#endif // WITH_DEV_AUTOMATION_TESTS
