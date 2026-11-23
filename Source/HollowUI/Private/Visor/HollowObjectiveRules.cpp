#include "Visor/HollowObjectiveRules.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowObjectiveRules)

#define LOCTEXT_NAMESPACE "HollowObjective"

namespace HollowObjectiveRulesPrivate
{
	constexpr int32 SecondsPerMinute = 60;
	constexpr int32 ClockSecondDigits = 2;
	constexpr double CentimetresPerMetre = 100.0;
	/** Below this a direction has no reliable angle (the target on the view's axis). */
	constexpr double MinDirection = 1.e-4;
	/** A field of view stays inside this, in degrees (a degenerate camera never divides by zero). */
	constexpr double MinFovDegrees = 1.0;
	constexpr double MaxFovDegrees = 179.0;
}

EHollowObjectiveStep HollowObjectiveRules::PickStep(const FHollowObjectiveFacts& Facts)
{
	if (Facts.bInRaid)
	{
		switch (Facts.Phase)
		{
		case EHollowRaidPhase::Assembling:
		case EHollowRaidPhase::Deploying:
			return EHollowObjectiveStep::Deploying;
		case EHollowRaidPhase::Debrief:
			return EHollowObjectiveStep::RaidOver;
		case EHollowRaidPhase::InRaid:
		case EHollowRaidPhase::Extraction:
			break;
		}
		if (!Facts.bAlive)
		{
			return EHollowObjectiveStep::Down;
		}
		if (Facts.Phase == EHollowRaidPhase::Extraction)
		{
			return EHollowObjectiveStep::HoldZone;
		}
		if (Facts.bRitual)
		{
			return EHollowObjectiveStep::GuardRitual;
		}
		if (!Facts.bCoreLifted)
		{
			return EHollowObjectiveStep::FindCore;
		}
		return Facts.bPurifiableCore && Facts.bAltarIdle ? EHollowObjectiveStep::PurifyOrExtract : EHollowObjectiveStep::DeliverCore;
	}
	if (Facts.bInHub)
	{
		if (!Facts.bPrepared)
		{
			return EHollowObjectiveStep::HubPrepare;
		}
		return Facts.bHost ? EHollowObjectiveStep::HubLaunch : EHollowObjectiveStep::HubWaitHost;
	}
	return EHollowObjectiveStep::None;
}

TArray<EHollowWaypointKind> HollowObjectiveRules::WaypointsFor(const EHollowObjectiveStep Step)
{
	switch (Step)
	{
	case EHollowObjectiveStep::HubPrepare:
	case EHollowObjectiveStep::HubLaunch:
	case EHollowObjectiveStep::HubWaitHost:
		return {EHollowWaypointKind::Contracts, EHollowWaypointKind::Workbench, EHollowWaypointKind::Locker};
	case EHollowObjectiveStep::FindCore:
		return {EHollowWaypointKind::Vault, EHollowWaypointKind::Extraction};
	case EHollowObjectiveStep::PurifyOrExtract:
	case EHollowObjectiveStep::GuardRitual:
		return {EHollowWaypointKind::Altar, EHollowWaypointKind::Extraction};
	case EHollowObjectiveStep::DeliverCore:
	case EHollowObjectiveStep::HoldZone:
		return {EHollowWaypointKind::Extraction};
	case EHollowObjectiveStep::None:
	case EHollowObjectiveStep::Deploying:
	case EHollowObjectiveStep::Down:
	case EHollowObjectiveStep::RaidOver:
		break;
	}
	return {};
}

FText HollowObjectiveRules::StepText(const EHollowObjectiveStep Step, const FHollowObjectiveFacts& Facts)
{
	switch (Step)
	{
	case EHollowObjectiveStep::None:
		break;
	case EHollowObjectiveStep::HubPrepare:
		return LOCTEXT("HubPrepare", "1/2  Check the workbench or the locker");
	case EHollowObjectiveStep::HubLaunch:
		return LOCTEXT("HubLaunch", "2/2  Go to the contracts console in the cockpit to launch the raid");
	case EHollowObjectiveStep::HubWaitHost:
		return LOCTEXT("HubWaitHost", "2/2  Go to the contracts console: the host launches the raid");
	case EHollowObjectiveStep::Deploying:
		return LOCTEXT("Deploying", "Deploying...");
	case EHollowObjectiveStep::FindCore:
		return LOCTEXT("FindCore", "Find the Aether core in the vault");
	case EHollowObjectiveStep::PurifyOrExtract:
		return FText::Format(LOCTEXT("PurifyOrExtract", "Purify the core at the Altar ({0}) or carry it to extraction ({1})"),
			Credits(Facts.PurifiedCredits), Credits(Facts.CoreCredits));
	case EHollowObjectiveStep::GuardRitual:
		return LOCTEXT("GuardRitual", "Guard the Altar until the ritual ends");
	case EHollowObjectiveStep::DeliverCore:
		return LOCTEXT("DeliverCore", "Deliver the core to the extraction zone");
	case EHollowObjectiveStep::HoldZone:
		return FText::Format(LOCTEXT("HoldZone", "Hold the zone until departure  {0}"), Clock(Facts.SecondsLeft));
	case EHollowObjectiveStep::Down:
		return LOCTEXT("Down", "You are down: the squad can still extract");
	case EHollowObjectiveStep::RaidOver:
		return LOCTEXT("RaidOver", "Raid over");
	}
	return FText::GetEmpty();
}

FHollowObjective HollowObjectiveRules::Pick(const FHollowObjectiveFacts& Facts)
{
	FHollowObjective Objective;
	Objective.Step = PickStep(Facts);
	Objective.Text = StepText(Objective.Step, Facts);
	Objective.Waypoints = WaypointsFor(Objective.Step);
	return Objective;
}

bool HollowObjectiveRules::IsTimed(const EHollowObjectiveStep Step)
{
	return Step == EHollowObjectiveStep::HoldZone;
}

FText HollowObjectiveRules::Clock(const int32 Seconds)
{
	using namespace HollowObjectiveRulesPrivate;
	FNumberFormattingOptions TwoDigits;
	TwoDigits.MinimumIntegralDigits = ClockSecondDigits;
	const int32 Clamped = FMath::Max(Seconds, 0);
	return FText::Format(INVTEXT("{0}:{1}"), FText::AsNumber(Clamped / SecondsPerMinute), FText::AsNumber(Clamped % SecondsPerMinute, &TwoDigits));
}

FText HollowObjectiveRules::Credits(const int32 Amount)
{
	return FText::Format(LOCTEXT("Credits", "{0} cr"), FText::AsNumber(Amount));
}

FVector2D HollowWaypointMath::ScreenDirection(const FVector& ViewLocation, const FRotator& ViewRotation, const FVector& Target)
{
	using namespace HollowObjectiveRulesPrivate;
	const FVector Local = ViewRotation.UnrotateVector(Target - ViewLocation);   // X forward, Y right, Z up
	const FVector2D Direction(Local.Y, -Local.Z);
	return Direction.Size() < MinDirection * FMath::Max(Local.Size(), 1.0) ? FVector2D(0.0, 1.0) : Direction.GetSafeNormal();
}

bool HollowWaypointMath::Project(const FVector& ViewLocation, const FRotator& ViewRotation, const float HorizontalFovDegrees, const FVector2D& Size,
	const FVector& Target, FVector2D& OutPosition)
{
	using namespace HollowObjectiveRulesPrivate;
	const FVector Local = ViewRotation.UnrotateVector(Target - ViewLocation);   // X forward, Y right, Z up
	const double HalfTan = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(static_cast<double>(HorizontalFovDegrees), MinFovDegrees, MaxFovDegrees) * 0.5));
	if (Local.X <= MinDirection || Size.X <= 0.0 || Size.Y <= 0.0)
	{
		return false;
	}
	const double Aspect = Size.X / Size.Y;
	const double X = Local.Y / (Local.X * HalfTan);            // -1 left edge .. 1 right edge
	const double Y = -Local.Z * Aspect / (Local.X * HalfTan);  // -1 top .. 1 bottom
	OutPosition = FVector2D((X + 1.0) * 0.5 * Size.X, (Y + 1.0) * 0.5 * Size.Y);
	return true;
}

FHollowWaypointPlacement HollowWaypointMath::Place(const FVector2D& Size, const float Margin, const bool bProjected, const FVector2D& Projected, const FVector2D& Direction)
{
	using namespace HollowObjectiveRulesPrivate;
	FHollowWaypointPlacement Placement;
	const FVector2D Centre = Size * 0.5;
	const FVector2D Half(FMath::Max(Centre.X - Margin, 0.0), FMath::Max(Centre.Y - Margin, 0.0));
	if (bProjected && FMath::Abs(Projected.X - Centre.X) <= Half.X && FMath::Abs(Projected.Y - Centre.Y) <= Half.Y)
	{
		Placement.Position = Projected;
		Placement.bOnScreen = true;
		return Placement;
	}
	FVector2D Toward = bProjected ? (Projected - Centre).GetSafeNormal() : Direction.GetSafeNormal();
	if (Toward.IsNearlyZero(MinDirection))
	{
		Toward = FVector2D(0.0, 1.0);
	}
	// The ray from the centre meets the inset rectangle where the first of its two sides stops it.
	const double ScaleX = FMath::Abs(Toward.X) > MinDirection ? Half.X / FMath::Abs(Toward.X) : TNumericLimits<double>::Max();
	const double ScaleY = FMath::Abs(Toward.Y) > MinDirection ? Half.Y / FMath::Abs(Toward.Y) : TNumericLimits<double>::Max();
	Placement.Position = Centre + Toward * FMath::Min(ScaleX, ScaleY);
	Placement.ArrowDegrees = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Toward.X, -Toward.Y)));
	return Placement;
}

int32 HollowWaypointMath::Metres(const FVector& From, const FVector& To)
{
	return FMath::RoundToInt32(FVector::Dist(From, To) / HollowObjectiveRulesPrivate::CentimetresPerMetre);
}

#undef LOCTEXT_NAMESPACE
