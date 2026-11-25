#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Localization/HollowLanguage.h"
#include "Settings/HollowGameUserSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowLanguageSettingsTest, "Hollow.Game.Settings.Language",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowLanguageSettingsTest::RunTest(const FString& Parameters)
{
	// LOC-01 (ADR-0048): the first launch takes the system's language and keeps it; a kept choice wins over the system;
	// -culture=xx is for one run only.
	bool bKeep = true;
	TestTrue(TEXT("First launch on a Russian system: Russian, kept"),
		UHollowGameUserSettings::ChooseLanguage({}, FString(), TEXT("ru-RU"), bKeep) == EHollowLanguage::Russian && bKeep);
	TestTrue(TEXT("First launch on a German system: English, kept"),
		UHollowGameUserSettings::ChooseLanguage({}, FString(), TEXT("de-DE"), bKeep) == EHollowLanguage::English && bKeep);
	TestTrue(TEXT("Kept Russian on an English system: Russian, nothing new to keep"),
		UHollowGameUserSettings::ChooseLanguage({}, TEXT("ru"), TEXT("en-US"), bKeep) == EHollowLanguage::Russian && !bKeep);
	TestTrue(TEXT("Kept English on a Russian system: English"),
		UHollowGameUserSettings::ChooseLanguage({}, TEXT("en"), TEXT("ru"), bKeep) == EHollowLanguage::English && !bKeep);
	TestTrue(TEXT("-culture=ru over a kept English: Russian for this run, not kept"),
		UHollowGameUserSettings::ChooseLanguage(EHollowLanguage::Russian, TEXT("en"), TEXT("en"), bKeep) == EHollowLanguage::Russian && !bKeep);
	TestTrue(TEXT("A broken kept value: the system's again, kept"),
		UHollowGameUserSettings::ChooseLanguage({}, TEXT("xx"), TEXT("ru"), bKeep) == EHollowLanguage::Russian && bKeep);

	// Switching without saving leaves the kept choice as it was (the console's Hollow.UI.Language without Keep).
	UHollowGameUserSettings* Settings = UHollowGameUserSettings::Get();
	if (!TestNotNull(TEXT("The game's settings class"), Settings))
	{
		return false;
	}
	const FString Kept = Settings->GetSavedLanguage();
	int32 Heard = 0;
	const FDelegateHandle Handle = HollowLanguage::OnApplied().AddLambda([&Heard] { ++Heard; });
	Settings->SetLanguage(EHollowLanguage::Russian, /*bSave=*/false);
	TestTrue(TEXT("Russian now, heard once"), Settings->GetLanguage() == EHollowLanguage::Russian && Heard == 1);
	Settings->SetLanguage(EHollowLanguage::Russian, /*bSave=*/false);
	TestEqual(TEXT("The same language again: nothing to hear"), Heard, 1);
	Settings->SetLanguage(EHollowLanguage::English, /*bSave=*/false);
	TestTrue(TEXT("English again"), Settings->GetLanguage() == EHollowLanguage::English && Heard == 2);
	TestEqual(TEXT("Nothing kept"), Settings->GetSavedLanguage(), Kept);
	HollowLanguage::OnApplied().Remove(Handle);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
