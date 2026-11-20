#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/LightComponent.h"
#include "Data/HollowRoomDefinition.h"
#include "EditorValidatorSubsystem.h"
#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Misc/DataValidation.h"
#include "Validation/HollowRoomLightingValidator.h"

namespace HollowLightValidatorTests
{
	const TCHAR* const HeroLevel = TEXT("/HollowSector_Docks/Rooms/LV_Docks_CoreVault.LV_Docks_CoreVault");
	const TCHAR* const PlainMap = TEXT("/Game/Hollow/Maps/L_Transition.L_Transition");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowLightValidatorTest, "Hollow.Editor.LightValidator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowLightValidatorTest::RunTest(const FString& Parameters)
{
	// UHollowRoomLightingValidator (ADR-0021): Data Validation knows a room's level by its definition, passes the
	// shipped hero room and fails it once its lights break the budget (here: every light casting shadows).
	using namespace HollowLightValidatorTests;
	UWorld* Level = LoadObject<UWorld>(nullptr, HeroLevel);
	UWorld* Plain = LoadObject<UWorld>(nullptr, PlainMap);
	UEditorValidatorSubsystem* Validation = GEditor ? GEditor->GetEditorSubsystem<UEditorValidatorSubsystem>() : nullptr;
	if (!TestNotNull(TEXT("LV_Docks_CoreVault"), Level) || !TestNotNull(TEXT("L_Transition"), Plain) || !TestNotNull(TEXT("Editor validation"), Validation))
	{
		return false;
	}
	TestNotNull(TEXT("A room's level is found by its definition"), UHollowRoomLightingValidator::FindRoomFor(*Level));
	TestNull(TEXT("Any other map is not a room"), UHollowRoomLightingValidator::FindRoomFor(*Plain));
	FDataValidationContext Shipped;
	TestTrue(TEXT("The shipped hero room passes"), Validation->IsObjectValidWithContext(Level, Shipped) != EDataValidationResult::Invalid);

	// Over budget in memory only (never saved): every light casts shadows.
	TArray<TPair<ULightComponent*, bool>> Restore;
	for (const AActor* Actor : Level->PersistentLevel->Actors)
	{
		TInlineComponentArray<ULightComponent*> Lights(Actor);
		for (ULightComponent* Light : Lights)
		{
			Restore.Emplace(Light, static_cast<bool>(Light->CastShadows));
			Light->CastShadows = true;
		}
	}
	FDataValidationContext Broken;
	TestTrue(TEXT("Over budget: invalid"), Validation->IsObjectValidWithContext(Level, Broken) == EDataValidationResult::Invalid);
	TestTrue(TEXT("...saying which budget and why"), Broken.GetIssues().ContainsByPredicate([](const FDataValidationContext::FIssue& Issue)
	{
		return Issue.Message.ToString().Contains(TEXT("Light budget of DA_Room_Docks_CoreVault")) && Issue.Message.ToString().Contains(TEXT("shadowed"));
	}));
	for (const TPair<ULightComponent*, bool>& Entry : Restore)
	{
		Entry.Key->CastShadows = Entry.Value;
	}
	FDataValidationContext Restored;
	TestTrue(TEXT("Restored: valid again"), Validation->IsObjectValidWithContext(Level, Restored) != EDataValidationResult::Invalid);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
