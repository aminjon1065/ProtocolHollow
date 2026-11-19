#pragma once

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "Tests/AutomationCommon.h"
#include "UObject/Package.h"

/**
 * Test fixture: a transient game world with one actor that owns a Hollow ability system and the given attribute sets.
 * Actors are initialized, so components behave as in a running game.
 */
class FHollowGASTestRig
{
public:
	template <typename... TAttributeSets>
	bool Create(FAutomationTestBase& Test)
	{
		if (!World.CreateTestWorld(EWorldType::Game))
		{
			World.ForwardErrorMessages(&Test);
			return false;
		}
		World.GetTestWorld()->InitializeActorsForPlay(FURL());

		Owner = World.GetTestWorld()->SpawnActor<AActor>();
		AbilitySystem = NewObject<UHollowAbilitySystemComponent>(Owner, TEXT("AbilitySystem"));
		AbilitySystem->RegisterComponent();
		(AbilitySystem->AddAttributeSetSubobject(NewObject<TAttributeSets>(Owner)), ...);
		AbilitySystem->InitAbilityActorInfo(Owner, Owner);
		return true;
	}

	/** Applies an instant effect with a single modifier, like an authored gameplay effect would. */
	void Apply(const FGameplayAttribute& Attribute, const EGameplayModOp::Type Operation, const float Magnitude) const
	{
		UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
		Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
		FGameplayModifierInfo& Modifier = Effect->Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = Operation;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Magnitude));
		AbilitySystem->ApplyGameplayEffectToSelf(Effect, 1.f, AbilitySystem->MakeEffectContext());
	}

	float Value(const FGameplayAttribute& Attribute) const
	{
		return AbilitySystem->GetNumericAttribute(Attribute);
	}

	FTestWorldWrapper World;
	AActor* Owner = nullptr;
	UHollowAbilitySystemComponent* AbilitySystem = nullptr;
};

#endif // WITH_DEV_AUTOMATION_TESTS
