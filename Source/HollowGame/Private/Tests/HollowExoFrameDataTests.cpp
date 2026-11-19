#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/HollowAbilitySet.h"
#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Attributes/HollowHealthSet.h"
#include "Attributes/HollowResonanceSet.h"
#include "Attributes/HollowSurvivalSet.h"
#include "Attributes/HollowToolSet.h"
#include "Engine/World.h"
#include "Framework/HollowPlayerState.h"
#include "GameplayEffect.h"
#include "Tests/AutomationCommon.h"
#include "UObject/UnrealType.h"

namespace HollowExoFrameDataTests
{
	const TCHAR* const AbilitySetPath = TEXT("/Game/Hollow/Characters/ExoFrame/DA_AbilitySet_ExoFrame.DA_AbilitySet_ExoFrame");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowExoFrameInitAttributesTest, "Hollow.Game.ExoFrame.InitAttributes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowExoFrameInitAttributesTest::RunTest(const FString& Parameters)
{
	const UHollowAbilitySet* AbilitySet = LoadObject<UHollowAbilitySet>(nullptr, HollowExoFrameDataTests::AbilitySetPath);
	if (!TestNotNull(TEXT("DA_AbilitySet_ExoFrame exists"), AbilitySet))
	{
		return false;
	}

	// 1. The init effect overrides every non-meta attribute of every player attribute set.
	TSet<FGameplayAttribute> Initialized;
	for (const FHollowAbilitySetEffect& Entry : AbilitySet->GrantedEffects)
	{
		if (const UGameplayEffect* Effect = Entry.GameplayEffect ? Entry.GameplayEffect->GetDefaultObject<UGameplayEffect>() : nullptr)
		{
			for (const FGameplayModifierInfo& Modifier : Effect->Modifiers)
			{
				if (Modifier.ModifierOp == EGameplayModOp::Override)
				{
					Initialized.Add(Modifier.Attribute);
				}
			}
		}
	}
	for (const UClass* SetClass : { UHollowHealthSet::StaticClass(), UHollowSurvivalSet::StaticClass(),
		UHollowResonanceSet::StaticClass(), UHollowToolSet::StaticClass() })
	{
		const UHollowAttributeSetBase* SetDefaults = GetDefault<UHollowAttributeSetBase>(const_cast<UClass*>(SetClass));
		for (TFieldIterator<FStructProperty> It(SetClass); It; ++It)
		{
			if (It->Struct != FGameplayAttributeData::StaticStruct())
			{
				continue;
			}
			const FGameplayAttribute Attribute(*It);
			if (!SetDefaults->IsMetaAttribute(Attribute))
			{
				TestTrue(Attribute.GetName() + TEXT(" is initialized by the ExoFrame ability set"), Initialized.Contains(Attribute));
			}
		}
	}

	// 2. Applied to a player state, the values respect the design rules of a fresh spawn.
	FTestWorldWrapper World;
	if (!World.CreateTestWorld(EWorldType::Game))
	{
		World.ForwardErrorMessages(this);
		return false;
	}
	World.GetTestWorld()->InitializeActorsForPlay(FURL());
	AHollowPlayerState* PlayerState = World.GetTestWorld()->SpawnActor<AHollowPlayerState>();
	UHollowAbilitySystemComponent* AbilitySystem = PlayerState->GetHollowAbilitySystemComponent();
	AbilitySystem->InitAbilityActorInfo(PlayerState, PlayerState);
	AbilitySet->GiveToAbilitySystem(AbilitySystem, nullptr);

	auto Value = [AbilitySystem](const FGameplayAttribute& Attribute) { return AbilitySystem->GetNumericAttribute(Attribute); };
	for (const UHollowAttributeSetBase* Set : { static_cast<const UHollowAttributeSetBase*>(AbilitySystem->GetSet<UHollowHealthSet>()),
		static_cast<const UHollowAttributeSetBase*>(AbilitySystem->GetSet<UHollowSurvivalSet>()),
		static_cast<const UHollowAttributeSetBase*>(AbilitySystem->GetSet<UHollowResonanceSet>()),
		static_cast<const UHollowAttributeSetBase*>(AbilitySystem->GetSet<UHollowToolSet>()) })
	{
		if (!TestNotNull(TEXT("Attribute set is registered"), Set))
		{
			return false;
		}
		for (const FHollowBoundedAttribute& Bounded : Set->GetBoundedAttributes())
		{
			TestTrue(Bounded.Max.GetName() + TEXT(" is positive"), Value(Bounded.Max) > 0.f);
		}
	}

	for (const FHollowBoundedAttribute& Full : {
		FHollowBoundedAttribute{ UHollowHealthSet::GetHealthAttribute(), UHollowHealthSet::GetMaxHealthAttribute() },
		FHollowBoundedAttribute{ UHollowSurvivalSet::GetStaminaAttribute(), UHollowSurvivalSet::GetMaxStaminaAttribute() },
		FHollowBoundedAttribute{ UHollowSurvivalSet::GetOxygenAttribute(), UHollowSurvivalSet::GetMaxOxygenAttribute() },
		FHollowBoundedAttribute{ UHollowSurvivalSet::GetThermalAttribute(), UHollowSurvivalSet::GetMaxThermalAttribute() },
		FHollowBoundedAttribute{ UHollowToolSet::GetToolFuelAttribute(), UHollowToolSet::GetMaxToolFuelAttribute() } })
	{
		TestEqual(Full.Current.GetName() + TEXT(" starts full"), Value(Full.Current), Value(Full.Max));
	}
	TestEqual(TEXT("Tool heat starts at zero"), Value(UHollowToolSet::GetToolHeatAttribute()), 0.f);
	TestEqual(TEXT("Resonance starts at zero"), Value(UHollowResonanceSet::GetResonanceAttribute()), 0.f);
	TestTrue(TEXT("Resonance gain is enabled"), Value(UHollowResonanceSet::GetResonanceGainMultiplierAttribute()) > 0.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
