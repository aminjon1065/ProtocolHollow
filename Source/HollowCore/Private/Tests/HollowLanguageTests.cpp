#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Localization/HollowLanguage.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowLanguageRulesTest, "Hollow.Core.Language.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FHollowLanguageRulesTest::RunTest(const FString& Parameters)
{
	// LOC-01 (ADR-0048): two languages, English first; any Russian culture of the system picks Russian, any other English.
	TestEqual(TEXT("English and Russian"), HollowLanguage::All().Num(), 2);
	TestTrue(TEXT("...English first"), HollowLanguage::All()[0] == EHollowLanguage::English);
	TestEqual(TEXT("en"), FString(HollowLanguage::CultureName(EHollowLanguage::English)), FString(TEXT("en")));
	TestEqual(TEXT("ru"), FString(HollowLanguage::CultureName(EHollowLanguage::Russian)), FString(TEXT("ru")));

	for (const TCHAR* Culture : {TEXT("ru"), TEXT("ru-RU"), TEXT("ru_RU"), TEXT("RU"), TEXT("ru-UA"), TEXT("ru-KZ")})
	{
		TestTrue(FString::Printf(TEXT("%s is Russian"), Culture), HollowLanguage::FromCulture(Culture) == EHollowLanguage::Russian);
	}
	for (const TCHAR* Culture : {TEXT("en"), TEXT("en-US"), TEXT("de-DE"), TEXT("uk"), TEXT("uk-UA"), TEXT("be"), TEXT("kk"), TEXT("rue"), TEXT("")})
	{
		TestTrue(FString::Printf(TEXT("'%s' is English"), Culture), HollowLanguage::FromCulture(Culture) == EHollowLanguage::English);
	}

	TestTrue(TEXT("Parse en / RU"), HollowLanguage::Parse(TEXT("en")) == EHollowLanguage::English && HollowLanguage::Parse(TEXT("RU")) == EHollowLanguage::Russian);
	TestFalse(TEXT("...not a culture of the game"), HollowLanguage::Parse(TEXT("de")).IsSet() || HollowLanguage::Parse(TEXT("ru-RU")).IsSet() || HollowLanguage::Parse(TEXT("")).IsSet());

	TestTrue(TEXT("The button cycles English -> Russian -> English"), HollowLanguage::Next(EHollowLanguage::English) == EHollowLanguage::Russian
		&& HollowLanguage::Next(EHollowLanguage::Russian) == EHollowLanguage::English);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
