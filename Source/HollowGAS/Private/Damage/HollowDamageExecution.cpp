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
	const float Penetration = Spec.GetSetByCallerMagnitude(HollowTags::Data_ArmorPenetration, /*WarnIfNotFound=*/false, /*DefaultIfNotFound=*/0.f);
	const float ExposedPenetration = Spec.GetSetByCallerMagnitude(HollowTags::Data_ExposedPenetration, /*WarnIfNotFound=*/false, /*DefaultIfNotFound=*/0.f);
	const FHollowDamageOutcome Outcome = HollowDamageRules::Resolve(Base, Armor, Evaluate.TargetTags, DamageType, Penetration, ExposedPenetration);

	UE_LOG(LogHollowGAS, Verbose, TEXT("Damage %s: %.1f -> %.1f (armor %.1f, penetration %.2f%s%s)"), *DamageType.ToString(), Base, Outcome.Damage, Armor,
		Penetration, Outcome.bExposed ? TEXT(", exposed") : TEXT(""), Outcome.bImmune ? TEXT(", immune") : TEXT(""));
	if (Outcome.Damage > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UHollowHealthSet::GetIncomingDamageAttribute(), EGameplayModOp::Additive, Outcome.Damage));
	}
}

UHollowDamageEffect::UHollowDamageEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayEffectExecutionDefinition& Execution = Executions.AddDefaulted_GetRef();
	Execution.CalculationClass = UHollowDamageExecution::StaticClass();
}
