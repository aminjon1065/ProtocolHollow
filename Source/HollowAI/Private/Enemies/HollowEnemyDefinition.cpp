#include "Enemies/HollowEnemyDefinition.h"

#include "Abilities/HollowAbilitySet.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "StateTree.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowEnemyDefinition)

#define LOCTEXT_NAMESPACE "HollowEnemyDefinition"

const FPrimaryAssetType UHollowEnemyDefinition::AssetType(TEXT("HollowEnemy"));

FPrimaryAssetId UHollowEnemyDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, GetFName());
}

#if WITH_EDITOR
EDataValidationResult UHollowEnemyDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	auto Fail = [&Context, &Result](const FText& Message)
	{
		Context.AddError(Message);
		Result = EDataValidationResult::Invalid;
	};
	if (!AbilitySet || !StateTree || !NavigationFilter)
	{
		Fail(LOCTEXT("Missing", "An enemy needs an ability set, a StateTree and a navigation filter."));
	}
	if (LoseSightRadius < SightRadius)
	{
		Fail(LOCTEXT("LoseSight", "LoseSightRadius must be at least SightRadius."));
	}
	if (ChaseStopDistance >= EngageRange)
	{
		Fail(LOCTEXT("ChaseStop", "ChaseStopDistance must be below EngageRange, or a finished chase may not be in strike range."));
	}
	if (!MeleeEvent.IsValid() || (CanBreach() && !BreachEvent.IsValid()))
	{
		Fail(LOCTEXT("Events", "MeleeEvent and BreachEvent must name the events of its attack abilities."));
	}
	if (FootstepSound.IsValid() && (!FootstepCadence.IsValid() || FootstepCadence.MinSpeed > PatrolSpeed))
	{
		Fail(LOCTEXT("Footsteps", "FootstepCadence needs an Interval and a ReferenceSpeed, and a MinSpeed no higher than PatrolSpeed (a patrol is heard)."));
	}
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
