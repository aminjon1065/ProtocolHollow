#include "Tests/HollowGameTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Character/HollowExoFrameCharacter.h"
#include "Framework/HollowPlayerController.h"
#include "Tags/HollowGameplayTags.h"
#include "Tests/HollowLocalizationTestUtils.h"
#include "UObject/Package.h"
#include "ViewModels/HollowVisorContext.h"
#include "Visor/HollowAcousticAlertWidget.h"
#include "Visor/HollowVisorStyle.h"

namespace HollowAcousticAlertVisorTests
{
	const TCHAR* const VisorPath = TEXT("/Game/Hollow/UI/Visor/W_Visor.W_Visor_C");
	const TCHAR* const AlertName = TEXT("AcousticAlert");
	/** The alert lasts a few seconds (create_visor_assets.py): long enough to read, gone before the next one may come. */
	constexpr float MinSeconds = 2.f;
	constexpr float MaxSeconds = 5.f;
	constexpr float Step = 0.1f;
	const FVector2D ViewSize(1920.0, 1080.0);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowVisorAcousticAlertTest, "Hollow.UI.Visor.AcousticAlert",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowVisorAcousticAlertTest::RunTest(const FString& Parameters)
{
	// POLISH-01 (ADR-0053): the real W_Visor shows ACOUSTIC ALERT when the server tells this player's controller that an
	// enemy answered its noise (Client_AcousticAlert), in the alert colour, in Russian at once on a language switch,
	// never cut, steady for DA_VisorStyle's seconds; the waypoints keep off it.
	using namespace HollowAcousticAlertVisorTests;
	using namespace HollowLocalizationTests;
	FTestWorldWrapper TestWorld;
	UWorld* World = HollowGameTests::CreateWorld(*this, TestWorld);
	const TSubclassOf<UUserWidget> VisorClass = LoadClass<UUserWidget>(nullptr, VisorPath);
	AHollowExoFrameCharacter* Character = World ? HollowGameTests::SpawnExoFrame(*this, World) : nullptr;
	if (!World || !TestNotNull(TEXT("W_Visor"), VisorClass.Get()) || !TestNotNull(TEXT("ExoFrame"), Character))
	{
		return false;
	}
	AddExpectedMessage(TEXT("InputConfig is not set"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, /*Occurrences=*/0, /*IsRegex=*/false);
	AHollowPlayerController* Controller = HollowGameTests::PossessAsPlayer<AHollowPlayerController>(World, Character, /*bLocalPlayer=*/true);
	UHollowVisorContext* Context = NewObject<UHollowVisorContext>(GetTransientPackage());
	Context->Initialize(Controller);
	UUserWidget* Visor = CreateWidget<UUserWidget>(World, VisorClass);
	UHollowAcousticAlertWidget* Alert = Visor ? Cast<UHollowAcousticAlertWidget>(Visor->WidgetTree->FindWidget(AlertName)) : nullptr;
	const UHollowVisorStyle* Style = Context->GetStyle();
	if (!TestNotNull(TEXT("W_Visor's AcousticAlert"), Alert) || !TestNotNull(TEXT("The visor style"), Style))
	{
		Context->Deinitialize();
		return false;
	}
	TestFalse(TEXT("DA_VisorStyle: the alert's text"), Style->AcousticAlertText.IsEmpty());
	TestTrue(TEXT("DA_VisorStyle: a few seconds"), Style->AcousticAlertSeconds >= MinSeconds && Style->AcousticAlertSeconds <= MaxSeconds);
	TestTrue(TEXT("DA_VisorStyle: the waypoints keep off it"), Style->WaypointKeepOut.Contains(FName(AlertName)));

	Alert->SetContext(Context);
	TestFalse(TEXT("No noise answered: hidden"), Alert->IsShown());
	Controller->Client_AcousticAlert(HollowTags::Noise_Voice_Shout);   // the server's call (a test world is its own server)
	TestTrue(TEXT("Answered: ACOUSTIC ALERT"), Alert->IsShown() && Alert->GetAlertText().EqualTo(Style->AcousticAlertText)
		&& Controller->GetAcousticAlerts() == 1 && Alert->Describe().Contains(HollowTags::Noise_Voice_Shout.GetTag().ToString()));
	{
		const FScopedLanguage Russian(EHollowLanguage::Russian);
		TestTrue(TEXT("Russian at once"), HasCyrillic(Alert->GetAlertText().ToString()));
		for (const FString& Overflow : FindOverflows(*Visor, ViewSize))
		{
			AddError(TEXT("Cut in Russian: ") + Overflow);
		}
	}
	Alert->SetContext(Context);   // laying the visor out built its Slate widgets anew
	Alert->ShowAlert(HollowTags::Noise_Tool_Welder);
	for (float Elapsed = 0.f; Elapsed < Style->AcousticAlertSeconds + Step * 5.f; Elapsed += Step)
	{
		++GFrameCounter;   // the timer manager ticks once per frame counter
		TestWorld.TickTestWorld(Step);
	}
	TestFalse(TEXT("Hidden after its seconds"), Alert->IsShown());
	Alert->SetContext(nullptr);
	Context->Deinitialize();
	return true;
}

#endif
