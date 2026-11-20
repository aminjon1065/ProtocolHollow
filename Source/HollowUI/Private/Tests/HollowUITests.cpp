#include "Tests/HollowGameTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Algo/Count.h"
#include "Attributes/HollowHealthSet.h"
#include "Attributes/HollowToolSet.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "Data/HollowToolDefinition.h"
#include "Doors/HollowDoor.h"
#include "Doors/HollowDoorDefinition.h"
#include "Doors/HollowDoorRules.h"
#include "Doors/HollowDoorStateComponent.h"
#include "Foundation/HollowPrimaryLayout.h"
#include "Foundation/HollowUISettings.h"
#include "GameplayEffect.h"
#include "HollowUITags.h"
#include "ICommonInputModule.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Menus/HollowSystemMenuWidget.h"
#include "Tags/HollowGameplayTags.h"
#include "Tools/HollowGA_ContinuousTool.h"
#include "UObject/Package.h"
#include "ViewModels/HollowAttributeWatch.h"
#include "ViewModels/HollowInputGlyphSet.h"
#include "ViewModels/HollowPromptModel.h"
#include "ViewModels/HollowToolTargetModel.h"
#include "ViewModels/HollowVisorContext.h"
#include "Visor/HollowStatBarWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

namespace HollowUITests
{
	// Content under test (create_ui_assets.py, create_tool_assets.py, create_gas_assets.py, create_door_assets.py).
	const TCHAR* const InitEffectClass = TEXT("/Game/Hollow/GAS/Effects/GE_ExoFrame_InitAttributes.GE_ExoFrame_InitAttributes_C");
	const TCHAR* const WelderClass = TEXT("/Game/Hollow/Tools/Welder/GA_Tool_Welder.GA_Tool_Welder_C");
	const TCHAR* const CutterClass = TEXT("/Game/Hollow/Tools/PlasmaCutter/GA_Tool_PlasmaCutter.GA_Tool_PlasmaCutter_C");
	const TCHAR* const BeamCueClass = TEXT("/Game/Hollow/GAS/Cues/GCN_Tool_Beam.GCN_Tool_Beam_C");
	const TCHAR* const DoorClass = TEXT("/Game/Hollow/World/Kit/Doors/BP_Door.BP_Door_C");
	const TCHAR* const InteractMappingContext = TEXT("/Game/Hollow/Core/Input/IMC_Raid.IMC_Raid");

	/** Test data. */
	constexpr float TickSeconds = 1.f / 60.f;
	constexpr float DoorDistance = 100.f;
	constexpr float WorkSeconds = 1.f;
	constexpr float FocusSeconds = 0.25f;
	constexpr float HalfHealth = 0.5f;
	constexpr int32 SettleFrames = 2;
	constexpr float LateWeld = 0.1f;

	void Advance(FTestWorldWrapper& World, const float Seconds)
	{
		// One engine frame per step: the timer manager ticks once per GFrameCounter.
		for (float Elapsed = 0.f; Elapsed < Seconds; Elapsed += TickSeconds)
		{
			++GFrameCounter;
			World.TickTestWorld(TickSeconds);
		}
	}

	void SetAttribute(UAbilitySystemComponent& AbilitySystem, const FGameplayAttribute& Attribute, const float Value)
	{
		UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
		Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
		FGameplayModifierInfo& Modifier = Effect->Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Override;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Value));
		AbilitySystem.ApplyGameplayEffectToSelf(Effect, 1.f, AbilitySystem.MakeEffectContext());
	}

	/** A local player's ExoFrame with its real starting attributes. */
	struct FPlayerRig
	{
		FTestWorldWrapper World;
		AHollowExoFrameCharacter* Character = nullptr;
		APlayerController* Controller = nullptr;
		UHollowAbilitySystemComponent* AbilitySystem = nullptr;
	};

	bool BuildPlayer(FAutomationTestBase& Test, FPlayerRig& Rig)
	{
		UWorld* World = HollowGameTests::CreateWorld(Test, Rig.World);
		Rig.Character = HollowGameTests::SpawnExoFrame(Test, World);
		const TSubclassOf<UGameplayEffect> InitClass = LoadClass<UGameplayEffect>(nullptr, InitEffectClass);
		if (!Test.TestNotNull(TEXT("ExoFrame"), Rig.Character) || !Test.TestNotNull(TEXT("GE_ExoFrame_InitAttributes"), InitClass.Get()))
		{
			return false;
		}
		// A local player sets up input; the bare C++ class has no InputConfig (BP_ExoFrame data), which it reports.
		Test.AddExpectedMessage(TEXT("InputConfig is not set"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, /*Occurrences=*/0, /*IsRegex=*/false);
		Rig.Controller = HollowGameTests::PossessAsPlayer(World, Rig.Character, /*bLocalPlayer=*/true);
		Rig.Controller->SetControlRotation(FRotator::ZeroRotator);
		Rig.AbilitySystem = Rig.Character->GetHollowAbilitySystemComponent();
		if (!Test.TestNotNull(TEXT("Ability system"), Rig.AbilitySystem))
		{
			return false;
		}
		Rig.AbilitySystem->ApplyGameplayEffectToSelf(InitClass.GetDefaultObject(), 1.f, Rig.AbilitySystem->MakeEffectContext());
		return true;
	}

	/** The real door, its leaf across the player's eye line DoorDistance ahead. */
	AHollowDoor* SpawnDoorAhead(FAutomationTestBase& Test, const FPlayerRig& Rig)
	{
		const TSubclassOf<AHollowDoor> Class = LoadClass<AHollowDoor>(nullptr, DoorClass);
		if (!Test.TestNotNull(TEXT("BP_Door"), Class.Get()))
		{
			return nullptr;
		}
		FVector Eyes;
		FRotator View;
		Rig.Character->GetActorEyesViewPoint(Eyes, View);
		AHollowDoor* Door = Rig.Character->GetWorld()->SpawnActor<AHollowDoor>(Class, FTransform(Eyes + FVector::ForwardVector * DoorDistance));
		const float DoorHeight = Door->GetLeaf()->Bounds.BoxExtent.Z * 2.f;
		Door->SetActorLocation(Door->GetActorLocation() - FVector(0.0, 0.0, DoorHeight * 0.5f));   // eye level at mid-height
		Door->DispatchBeginPlay();
		return Door;
	}

	FGameplayAbilitySpecHandle GiveTool(FAutomationTestBase& Test, UHollowAbilitySystemComponent& AbilitySystem, const TCHAR* ClassPath, const FGameplayTag& InputTag)
	{
		const TSubclassOf<UGameplayAbility> Class = LoadClass<UGameplayAbility>(nullptr, ClassPath);
		if (!Test.TestNotNull(ClassPath, Class.Get()))
		{
			return FGameplayAbilitySpecHandle();
		}
		FGameplayAbilitySpec Spec(Class, 1);
		Spec.GetDynamicSpecSourceTags().AddTag(InputTag);
		return AbilitySystem.GiveAbility(Spec);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowUIAttributeWatchTest, "Hollow.UI.AttributeWatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowUIAttributeWatchTest::RunTest(const FString& Parameters)
{
	using namespace HollowUITests;
	TestEqual(TEXT("Ratio without a maximum is 0"), FHollowAttributeWatch::Ratio(5.f, 0.f), 0.f);
	TestEqual(TEXT("Ratio is clamped above"), FHollowAttributeWatch::Ratio(150.f, 100.f), 1.f);
	TestEqual(TEXT("Ratio is clamped below"), FHollowAttributeWatch::Ratio(-5.f, 100.f), 0.f);

	FPlayerRig Rig;
	if (!BuildPlayer(*this, Rig))
	{
		return false;
	}
	UHollowAbilitySystemComponent& AbilitySystem = *Rig.AbilitySystem;
	const float MaxHealth = AbilitySystem.GetNumericAttribute(UHollowHealthSet::GetMaxHealthAttribute());
	int32 Calls = 0;
	float LastValue = -1.f;
	float LastMax = -1.f;
	FHollowAttributeWatch Watch;
	Watch.Bind(&AbilitySystem, UHollowHealthSet::GetHealthAttribute(), UHollowHealthSet::GetMaxHealthAttribute(), [&](const float Value, const float Max)
	{
		++Calls;
		LastValue = Value;
		LastMax = Max;
	});
	TestEqual(TEXT("Binding reports the current values once"), Calls, 1);
	TestTrue(TEXT("...starting at full health"), MaxHealth > 0.f && LastValue == MaxHealth && LastMax == MaxHealth);

	SetAttribute(AbilitySystem, UHollowHealthSet::GetHealthAttribute(), MaxHealth * HalfHealth);
	TestEqual(TEXT("One callback per change"), Calls, 2);
	TestEqual(TEXT("Half health -> ratio 0.5"), Watch.GetRatio(), HalfHealth);

	SetAttribute(AbilitySystem, UHollowHealthSet::GetMaxHealthAttribute(), MaxHealth * HalfHealth);
	TestEqual(TEXT("The maximum is followed too"), LastMax, MaxHealth * HalfHealth);
	TestEqual(TEXT("...and the ratio with it"), Watch.GetRatio(), 1.f);

	Watch.Unbind();
	SetAttribute(AbilitySystem, UHollowHealthSet::GetHealthAttribute(), 0.f);
	TestEqual(TEXT("No callbacks after unbinding"), Calls, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowUIInputGlyphsTest, "Hollow.UI.InputGlyphs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowUIInputGlyphsTest::RunTest(const FString& Parameters)
{
	using namespace HollowUITests;
	const UHollowUISettings* Settings = GetDefault<UHollowUISettings>();
	const UInputAction* Interact = Settings->InteractAction.LoadSynchronous();
	const UHollowInputGlyphSet* Glyphs = Settings->InputGlyphs.LoadSynchronous();
	const UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, InteractMappingContext);
	if (!TestNotNull(TEXT("IA_Interact"), Interact) || !TestNotNull(TEXT("DA_InputGlyphs"), Glyphs) || !TestNotNull(TEXT("IMC_Raid"), Context))
	{
		return false;
	}
	// The keys the game maps to Interact, as the Enhanced Input subsystem would report them.
	TArray<FKey> Keys;
	for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
	{
		if (Mapping.Action == Interact)
		{
			Keys.Add(Mapping.Key);
		}
	}
	const FKey Keyboard = UHollowInputGlyphSet::PickKey(Keys, ECommonInputType::MouseAndKeyboard);
	const FKey Gamepad = UHollowInputGlyphSet::PickKey(Keys, ECommonInputType::Gamepad);
	TestTrue(TEXT("Keyboard prompt uses a keyboard key"), Keyboard.IsValid() && !Keyboard.IsGamepadKey());
	TestTrue(TEXT("Gamepad prompt uses a gamepad key"), Gamepad.IsValid() && Gamepad.IsGamepadKey());
	TestFalse(TEXT("No key for touch"), UHollowInputGlyphSet::PickKey(Keys, ECommonInputType::Touch).IsValid());
	TestEqual(TEXT("E is shown as E"), Glyphs->GetLabel(EKeys::E).ToString(), FString(TEXT("E")));
	TestEqual(TEXT("Gamepad face button left is shown as X"), Glyphs->GetLabel(EKeys::Gamepad_FaceButton_Left).ToString(), FString(TEXT("X")));
	TestEqual(TEXT("Unlisted keys fall back to the engine's short name"), Glyphs->GetLabel(EKeys::Z).ToString(), EKeys::Z.GetDisplayName(false).ToString());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowUIVisorContextTest, "Hollow.UI.VisorContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowUIVisorContextTest::RunTest(const FString& Parameters)
{
	using namespace HollowUITests;
	FPlayerRig Rig;
	if (!BuildPlayer(*this, Rig))
	{
		return false;
	}
	UHollowVisorContext* Context = NewObject<UHollowVisorContext>(GetTransientPackage());
	Context->Initialize(Rig.Controller);
	int32 SourceChanges = 0;
	Context->OnSourcesChanged.AddLambda([&SourceChanges] { ++SourceChanges; });
	TestTrue(TEXT("Follows the possessed ExoFrame"), Context->GetCharacter() == Rig.Character);
	TestTrue(TEXT("...and its ability system"), Context->GetAbilitySystem() == Rig.AbilitySystem);
	TestTrue(TEXT("Mouse and keyboard without a local player"), Context->GetInputType() == ECommonInputType::MouseAndKeyboard);

	// Prompt: the real door ahead, focused by the interaction component's own timer.
	const AHollowDoor* Door = SpawnDoorAhead(*this, Rig);
	if (!Door)
	{
		return false;
	}
	Advance(Rig.World, FocusSeconds);
	const UHollowPromptModel* Prompt = Context->GetPrompt();
	TestTrue(TEXT("The door is focused"), Prompt->IsVisible() && Prompt->GetFocus() == Door);
	TestTrue(TEXT("...with its prompt"), Prompt->GetPrompt().EqualTo(Door->GetDoorState()->GetDefinition()->PromptOpen));

	// Possession changes.
	Rig.Controller->UnPossess();
	TestTrue(TEXT("Unpossessed: sources changed"), SourceChanges > 0);
	TestTrue(TEXT("...no character, no ability system, no prompt"), !Context->GetCharacter() && !Context->GetAbilitySystem() && !Prompt->IsVisible());
	AHollowExoFrameCharacter* Second = HollowGameTests::SpawnExoFrame(*this, Rig.Character->GetWorld(), FTransform(FVector::LeftVector * DoorDistance * 4.f));
	Rig.Controller->Possess(Second);
	TestTrue(TEXT("A new pawn is followed"), Context->GetCharacter() == Second && Context->GetAbilitySystem() == Rig.AbilitySystem);
	Context->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowUIToolTargetTest, "Hollow.UI.ToolTargetStatus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowUIToolTargetTest::RunTest(const FString& Parameters)
{
	using namespace HollowUITests;
	FPlayerRig Rig;
	const UClass* BeamCue = LoadClass<AActor>(nullptr, BeamCueClass);   // loaded up front: a cue still loading would spawn into the next test
	if (!TestNotNull(TEXT("GCN_Tool_Beam"), BeamCue) || !BuildPlayer(*this, Rig))
	{
		return false;
	}
	UHollowAbilitySystemComponent& AbilitySystem = *Rig.AbilitySystem;
	const FGameplayAbilitySpecHandle Welder = GiveTool(*this, AbilitySystem, WelderClass, HollowTags::Input_SecondaryUse);
	const FGameplayAbilitySpecHandle Cutter = GiveTool(*this, AbilitySystem, CutterClass, HollowTags::Input_PrimaryUse);
	AHollowDoor* Door = SpawnDoorAhead(*this, Rig);
	if (!Welder.IsValid() || !Cutter.IsValid() || !Door)
	{
		return false;
	}
	const UHollowDoorDefinition* DoorDefinition = Door->GetDoorState()->GetDefinition();
	const UHollowToolDefinition* WelderTool = CastChecked<UHollowGA_ContinuousTool>(AbilitySystem.FindAbilitySpecFromHandle(Welder)->Ability)->GetToolDefinition();
	const UHollowUISettings* Settings = GetDefault<UHollowUISettings>();
	UHollowVisorContext* Context = NewObject<UHollowVisorContext>(GetTransientPackage());
	Context->Initialize(Rig.Controller);
	const UHollowToolTargetModel* Target = Context->GetToolTarget();
	TestFalse(TEXT("Hidden while no tool runs"), Target->IsVisible());

	// Welding: the bar follows the door's weld, with the seal threshold marked.
	AbilitySystem.AbilityInputTagPressed(HollowTags::Input_SecondaryUse);
	TestTrue(TEXT("The welder runs and owns the tool-active tag"), AbilitySystem.HasMatchingGameplayTag(HollowTags::State_Tool_Active));
	Advance(Rig.World, TickSeconds * SettleFrames);
	TestTrue(TEXT("The bar appears at once"), Target->IsVisible() && Target->GetTarget() == Door);
	TestTrue(TEXT("...labelled with the tool's action"), Target->GetActionLabel().EqualTo(WelderTool->ActionLabel) && !WelderTool->ActionLabel.IsEmpty());
	TestEqual(TEXT("...marking the seal threshold"), Target->GetStatus().Marker, DoorDefinition->SealThreshold);
	Advance(Rig.World, WorkSeconds);
	const float Welded = Door->GetDoorState()->GetWeld();
	TestTrue(FString::Printf(TEXT("Welding fills the bar like the door (%.3f vs %.3f)"), Target->GetStatus().Value, Welded),
		Welded > 0.f && FMath::IsNearlyEqual(Target->GetStatus().Value, Welded, DoorDefinition->WeldPerWorkUnit * WelderTool->WorkRate / Settings->TargetProbeRateHz));

	// Release: the bar lingers, still taking values that replicate late, then hides.
	AbilitySystem.AbilityInputTagReleased(HollowTags::Input_SecondaryUse);
	Advance(Rig.World, TickSeconds * SettleFrames);
	TestTrue(TEXT("The bar lingers after release"), Target->IsVisible());
	Door->GetDoorState()->ApplyTool(EHollowDoorEvent::Weld, LateWeld / DoorDefinition->WeldPerWorkUnit);   // a weld update after the release
	Advance(Rig.World, 1.f / Settings->TargetProbeRateHz + TickSeconds * SettleFrames);
	TestEqual(TEXT("...and shows a value that arrives after the release"), Target->GetStatus().Value, Door->GetDoorState()->GetWeld());
	Advance(Rig.World, Settings->TargetLingerSeconds);
	TestFalse(TEXT("...then hides"), Target->IsVisible());

	// Cutting: the bar drains.
	AbilitySystem.AbilityInputTagPressed(HollowTags::Input_PrimaryUse);
	Advance(Rig.World, WorkSeconds);
	TestTrue(TEXT("Cutting drains the bar"), Target->IsVisible() && Target->GetStatus().Value < Welded + LateWeld);
	AbilitySystem.AbilityInputTagReleased(HollowTags::Input_PrimaryUse);
	Advance(Rig.World, Settings->TargetLingerSeconds + TickSeconds * SettleFrames);

	// Nothing in reach (the door moved away; test controllers have no camera to turn): nothing to show.
	const FVector DoorHome = Door->GetActorLocation();
	Door->SetActorLocation(DoorHome + FVector::ForwardVector * WelderTool->Range * 2.f);
	AbilitySystem.AbilityInputTagPressed(HollowTags::Input_SecondaryUse);
	Advance(Rig.World, TickSeconds * SettleFrames);
	TestFalse(TEXT("No bar when the beam hits nothing"), Target->IsVisible());
	Door->SetActorLocation(DoorHome);
	Advance(Rig.World, 1.f / Settings->TargetProbeRateHz + TickSeconds * SettleFrames);
	TestTrue(TEXT("...and back when it hits the door again"), Target->IsVisible() && Target->GetTarget() == Door);
	AbilitySystem.AbilityInputTagReleased(HollowTags::Input_SecondaryUse);
	Context->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowUILayoutTest, "Hollow.UI.Layout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowUILayoutTest::RunTest(const FString& Parameters)
{
	using namespace HollowUITests;
	const UHollowUISettings* Settings = GetDefault<UHollowUISettings>();
	const TSubclassOf<UHollowPrimaryLayout> LayoutClass = Settings->PrimaryLayoutClass.LoadSynchronous();
	const TSubclassOf<UCommonActivatableWidget> VisorClass = Settings->VisorClass.LoadSynchronous();
	const TSubclassOf<UCommonActivatableWidget> MenuClass = Settings->SystemMenuClass.LoadSynchronous();
	FPlayerRig Rig;
	if (!TestNotNull(TEXT("W_PrimaryLayout"), LayoutClass.Get()) || !TestNotNull(TEXT("W_Visor"), VisorClass.Get())
		|| !TestNotNull(TEXT("W_SystemMenu"), MenuClass.Get()) || !BuildPlayer(*this, Rig))
	{
		return false;
	}
	// A game loads CommonUI's input data with the first local player's input subsystem; test worlds have no local player.
	ICommonInputModule::GetSettings().LoadData();
	// Test controllers have no ULocalPlayer, which CreateWidget(PlayerController) requires: the world owns the layout.
	UHollowPrimaryLayout* Layout = CreateWidget<UHollowPrimaryLayout>(Rig.Character->GetWorld(), LayoutClass);
	if (!TestNotNull(TEXT("Layout widget"), Layout))
	{
		return false;
	}
	const TSharedRef<SWidget> LayoutSlate = Layout->TakeWidget();   // the stacks' Slate switchers live as long as this (no viewport here)

	// The visor: game input, three attribute bars bound to the player's ability system.
	const UCommonActivatableWidget* Visor = Layout->PushWidgetToLayer(HollowUITags::UI_Layer_Game, VisorClass);
	TestTrue(TEXT("The visor is the Game layer's active widget"), Visor && Layout->GetLayer(HollowUITags::UI_Layer_Game)->GetActiveWidget() == Visor);
	const TOptional<FUIInputConfig> VisorInput = Visor ? Visor->GetDesiredInputConfig() : TOptional<FUIInputConfig>();
	TestTrue(TEXT("...asking for game input"), VisorInput.IsSet() && VisorInput->GetInputMode() == ECommonInputMode::Game);
	UHollowVisorContext* Context = NewObject<UHollowVisorContext>(GetTransientPackage());
	Context->Initialize(Rig.Controller);
	TArray<UHollowStatBarWidget*> Bars;
	if (Visor)
	{
		Visor->WidgetTree->ForEachWidget([&Bars](UWidget* Widget)
		{
			if (UHollowStatBarWidget* Bar = Cast<UHollowStatBarWidget>(Widget))
			{
				Bars.Add(Bar);
			}
		});
	}
	TestEqual(TEXT("Health, fuel and heat bars"), Bars.Num(), 3);
	for (UHollowStatBarWidget* Bar : Bars)
	{
		Bar->SetContext(Context);
		const float Expected = Rig.AbilitySystem->GetNumericAttribute(Bar->GetAttribute());
		TestTrue(FString::Printf(TEXT("%s bar shows its attribute (%.1f)"), *Bar->GetLabel().ToString(), Expected),
			Bar->GetAttribute().IsValid() && Bar->GetValue() == Expected);
	}

	// Overheating turns the heat bar (and only it) to its alert look.
	const auto CountAlerts = [&Bars] { return Algo::CountIf(Bars, [](const UHollowStatBarWidget* Bar) { return Bar->IsAlert(); }); };
	Rig.AbilitySystem->AddLooseGameplayTag(HollowTags::State_Tool_Overheated);
	const UHollowStatBarWidget* const* Heat = Bars.FindByPredicate([](const UHollowStatBarWidget* Bar) { return Bar->IsAlert(); });
	TestTrue(TEXT("Overheated: the heat bar alerts"), CountAlerts() == 1 && Heat && (*Heat)->GetAttribute() == UHollowToolSet::GetToolHeatAttribute());
	Rig.AbilitySystem->RemoveLooseGameplayTag(HollowTags::State_Tool_Overheated);
	TestEqual(TEXT("...and stops when it cools down"), static_cast<int32>(CountAlerts()), 0);

	// The system menu: menu input, focus on Resume, Back closes it.
	UHollowSystemMenuWidget* Menu = Cast<UHollowSystemMenuWidget>(Layout->PushWidgetToLayer(HollowUITags::UI_Layer_Menu, MenuClass));
	TestTrue(TEXT("The menu is active on the Menu layer"), Menu && Menu->IsActivated() && Layout->GetLayer(HollowUITags::UI_Layer_Menu)->GetActiveWidget() == Menu);
	if (Menu)
	{
		const TOptional<FUIInputConfig> MenuInput = Menu->GetDesiredInputConfig();
		TestTrue(TEXT("...asking for menu input"), MenuInput.IsSet() && MenuInput->GetInputMode() == ECommonInputMode::Menu);
		TestTrue(TEXT("...gamepad focus starts on Resume"), Menu->GetDesiredFocusTarget() == Menu->GetResumeButton());
		Menu->DeactivateWidget();   // what Back does (bIsBackHandler); the key path is checked in game (Hollow.UI.PressKey)
		TestFalse(TEXT("Deactivation closes it"), Menu->IsActivated());
	}
	TestTrue(TEXT("The visor stays active under the menu"), Visor && Visor->IsActivated());
	for (UHollowStatBarWidget* Bar : Bars)
	{
		Bar->SetContext(nullptr);
	}
	Context->Deinitialize();
	Layout->RemoveFromParent();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
