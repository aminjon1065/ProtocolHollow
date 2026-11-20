#include "Damage/HollowDamageExecution.h"

#include "Attributes/HollowHealthSet.h"
#include "Damage/HollowDamageMath.h"
#include "Damage/HollowDamageSettings.h"
#include "HollowGASLog.h"
#include "Tags/HollowGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowDamageExecution)

namespace HollowDamageExecutionPrivate
{
	struct FCaptures
	{
		FGameplayEffectAttributeCaptureDefinition Armor;

		FCaptures()
			: Armor(UHollowHealthSet::GetArmorAttribute(), EGameplayEffectAttributeCaptureSource::Target, /*bSnapshot=*/false)
		{
		}
	};

	const FCaptures& Captures()
	{
		static const FCaptures Instance;
		return Instance;
	}
}

UHollowDamageExecution::UHollowDamageExecution()
{
	RelevantAttributesToCapture.Add(HollowDamageExecutionPrivate::Captures().Armor);
}

void UHollowDamageExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	FAggregatorEvaluateParameters Evaluate;
	Evaluate.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	Evaluate.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	float Armor = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(HollowDamageExecutionPrivate::Captures().Armor, Evaluate, Armor);
	const float Base = Spec.GetSetByCallerMagnitude(HollowTags::Data_Damage, /*WarnIfNotFound=*/true, /*DefaultIfNotFound=*/0.f);

	const UHollowDamageEffect* Effect = Cast<UHollowDamageEffect>(Spec.Def);
	const FGameplayTag DamageType = Effect ? Effect->DamageType : FGameplayTag();
	const UHollowDamageSettings* Settings = GetDefault<UHollowDamageSettings>();
	const FHollowDamageTypeRule* Rule = Settings->FindRule(DamageType);
	// Immune by the type's rule, or to everything with the exact Hollow.State.Invulnerable (its children, such as the
	// stalker's .Kinetic, only count through their own rule).
	const bool bInvulnerable = Evaluate.TargetTags && Evaluate.TargetTags->HasTagExact(HollowTags::State_Invulnerable);
	const bool bImmune = bInvulnerable || (Rule && Rule->ImmunityTag.IsValid() && Evaluate.TargetTags && Evaluate.TargetTags->HasTag(Rule->ImmunityTag));
	const float Damage = HollowDamageMath::Mitigate(Base, Armor, Settings->ArmorConstant, Rule ? Rule->bArmorApplies : true, bImmune);

	UE_LOG(LogHollowGAS, Verbose, TEXT("Damage %s: %.1f -> %.1f (armor %.1f%s)"), *DamageType.ToString(), Base, Damage, Armor, bImmune ? TEXT(", immune") : TEXT(""));
	if (Damage > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UHollowHealthSet::GetIncomingDamageAttribute(), EGameplayModOp::Additive, Damage));
	}
}

UHollowDamageEffect::UHollowDamageEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayEffectExecutionDefinition& Execution = Executions.AddDefaulted_GetRef();
	Execution.CalculationClass = UHollowDamageExecution::StaticClass();
}
