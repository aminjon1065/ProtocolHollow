#include "Breach/HollowBreachRules.h"

#include "Breach/HollowBreachableDefinition.h"

namespace HollowBreachRulesPrivate
{
	constexpr float QuantizeScale = TNumericLimits<uint8>::Max();

	int32 ZoneCount(const UHollowBreachableDefinition& Definition)
	{
		return FMath::Min(Definition.Zones.Num(), FHollowBreachNetState::MaxZones);
	}
}

FHollowBreachServerState HollowBreachRules::MakeInitial(const UHollowBreachableDefinition& Definition)
{
	FHollowBreachServerState State;
	for (int32 Index = 0; Index < HollowBreachRulesPrivate::ZoneCount(Definition); ++Index)
	{
		State.Health.Add(Definition.Zones[Index].MaxHealth);
	}
	return State;
}

int32 HollowBreachRules::PickZone(const FHollowBreachServerState& State, const FVector& LocalPoint, const UHollowBreachableDefinition& Definition)
{
	int32 Best = INDEX_NONE;
	double BestDistance = TNumericLimits<double>::Max();
	for (int32 Index = 0; Index < FMath::Min(HollowBreachRulesPrivate::ZoneCount(Definition), State.Health.Num()); ++Index)
	{
		if (State.Net.IsZoneBroken(Index))
		{
			continue;
		}
		const double Distance = FVector::DistSquared(LocalPoint, Definition.Zones[Index].Center);
		if (Distance < BestDistance)   // strict: the lower index wins a tie
		{
			Best = Index;
			BestDistance = Distance;
		}
	}
	return Best;
}

EHollowBreachOutcome HollowBreachRules::ApplyHit(FHollowBreachServerState& InOut, const FHollowBreachHit& Hit, const UHollowBreachableDefinition& Definition)
{
	if (InOut.Net.State == EHollowBreachState::Breached || Hit.Amount <= 0.f)
	{
		return EHollowBreachOutcome::Rejected;
	}
	const int32 Zone = PickZone(InOut, Hit.LocalPoint, Definition);
	if (Zone == INDEX_NONE)
	{
		return EHollowBreachOutcome::Rejected;
	}

	InOut.Health[Zone] = FMath::Max(InOut.Health[Zone] - Hit.Amount, 0.f);
	InOut.Net.DamageStageQ = DamageStage(InOut, Definition);
	InOut.Net.State = EHollowBreachState::Weakened;
	if (InOut.Health[Zone] > 0.f)
	{
		return EHollowBreachOutcome::Damaged;
	}

	InOut.Net.ZoneMask |= static_cast<uint8>(1u << Zone);
	InOut.Net.SetImpactDirection(Hit.Direction);
	InOut.Net.BreachServerTime = Hit.ServerTime;
	++InOut.Net.TransitionSerial;   // wraps at 255: only "changed" matters
	if (IsPassageOpen(InOut.Net.ZoneMask, Definition))
	{
		InOut.Net.State = EHollowBreachState::Breached;
	}
	return EHollowBreachOutcome::ZoneBroken;
}

bool HollowBreachRules::IsPassageOpen(const uint8 ZoneMask, const UHollowBreachableDefinition& Definition)
{
	const uint8 Required = Definition.GetPassageMask();
	return Required != 0 && (ZoneMask & Required) == Required;
}

uint8 HollowBreachRules::DamageStage(const FHollowBreachServerState& State, const UHollowBreachableDefinition& Definition)
{
	const float Total = Definition.GetTotalHealth();
	if (Total <= 0.f)
	{
		return 0;
	}
	float Left = 0.f;
	for (const float Health : State.Health)
	{
		Left += Health;
	}
	const float Lost = FMath::Clamp(1.f - Left / Total, 0.f, 1.f);
	return static_cast<uint8>(FMath::RoundToInt(Lost * HollowBreachRulesPrivate::QuantizeScale));
}
