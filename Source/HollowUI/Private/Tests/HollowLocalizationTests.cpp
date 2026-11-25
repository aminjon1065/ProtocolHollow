#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/FontCache.h"
#include "Framework/Application/SlateApplication.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/TextLocalizationResource.h"
#include "Misc/Paths.h"
#include "Rendering/SlateRenderer.h"
#include "Tests/HollowLocalizationTestUtils.h"

namespace HollowLocalizationTests
{
	/** The visor's telemetry font (create_visor_assets.py); everything else draws with the text block's own (Roboto). */
	const TCHAR* const TelemetryFont = TEXT("/Engine/EngineFonts/DroidSansMono.DroidSansMono");

	/** Numbers as the Russian culture writes them (credits, metres, percent): its separators must be drawable too. */
	TArray<FString> RussianNumbers(const FCulturePtr& Russian)
	{
		FNumberFormattingOptions Decimal;
		Decimal.SetMinimumFractionalDigits(1);
		return {FText::AsNumber(1234567, nullptr, Russian).ToString(), FText::AsNumber(-1250, nullptr, Russian).ToString(),
			FText::AsNumber(12.5, &Decimal, Russian).ToString(), FText::AsPercent(0.42, nullptr, Russian).ToString()};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowLocalizationFontsTest, "Hollow.UI.Localization.Fonts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowLocalizationFontsTest::RunTest(const FString& Parameters)
{
	// LOC-01 (ADR-0048): every character of every Russian text (and of numbers written the Russian way) is in each face
	// of the fonts the UI draws with, without a fallback font: no boxes on Mac or Win64 (the faces ship in the pak).
	using namespace HollowLocalizationTests;
	if (!TestTrue(TEXT("Slate can measure and load fonts here"), CanMeasureText()))
	{
		return false;
	}
	const UFont* Roboto = Cast<UFont>(GetDefault<UTextBlock>()->GetFont().FontObject);
	const UFont* Telemetry = LoadObject<UFont>(nullptr, TelemetryFont);
	const FCulturePtr RussianCulture = FInternationalization::Get().GetCulture(TEXT("ru"));
	if (!TestNotNull(TEXT("The text block's font"), Roboto) || !TestNotNull(TEXT("DroidSansMono"), Telemetry)
		|| !TestTrue(TEXT("The Russian culture (ICU data)"), RussianCulture.IsValid()))
	{
		return false;
	}

	FTextLocalizationResource Russian;
	const FString Locres = FPaths::ProjectContentDir() / TEXT("Localization/Game/ru/Game.locres");
	if (!TestTrue(TEXT("ru/Game.locres loads"), Russian.LoadFromFile(Locres, /*Priority=*/0)))
	{
		return false;
	}
	TArray<FString> Texts;
	for (const TPair<FTextId, FTextLocalizationResource::FEntry>& Entry : Russian.Entries)
	{
		if (Entry.Value.LocalizedString.IsValid())
		{
			Texts.Add(*Entry.Value.LocalizedString);
		}
	}
	TestTrue(FString::Printf(TEXT("Russian texts (%d)"), Texts.Num()), Texts.Num() > 100);
	Texts.Append(RussianNumbers(RussianCulture));

	TMap<TCHAR, FString> Characters;   // each character once, with a text it is in
	for (const FString& Text : Texts)
	{
		for (const TCHAR Character : Text)
		{
			if (Character >= TCHAR(' '))
			{
				Characters.FindOrAdd(Character, Text);
			}
		}
	}

	FSlateFontCache& Cache = FSlateApplication::Get().GetRenderer()->GetFontCache().Get();
	int32 Faces = 0;
	TArray<FString> Problems;
	for (const UFont* Font : {Roboto, Telemetry})
	{
		const FCompositeFont* Composite = Font->GetCompositeFont();
		if (!TestNotNull(FString::Printf(TEXT("%s is a composite font"), *Font->GetName()), Composite))
		{
			continue;
		}
		for (const FTypefaceEntry& Face : Composite->DefaultTypeface.Fonts)
		{
			++Faces;
			for (const TPair<TCHAR, FString>& Character : Characters)
			{
				if (!Cache.CanLoadCodepoint(Face.Font, Character.Key))
				{
					Problems.Add(FString::Printf(TEXT("%s %s has no U+%04X (in \"%s\")"), *Font->GetName(), *Face.Name.ToString(),
						static_cast<uint32>(Character.Key), *Character.Value));
				}
			}
		}
	}
	for (const FString& Problem : Problems)
	{
		AddError(Problem);
	}
	TestTrue(FString::Printf(TEXT("Faces checked (%d) for %d characters"), Faces, Characters.Num()), Faces >= 2 && Characters.Num() > 60);
	return Problems.IsEmpty();
}

#endif // WITH_DEV_AUTOMATION_TESTS
