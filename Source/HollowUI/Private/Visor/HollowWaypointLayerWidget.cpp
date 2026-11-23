#include "Visor/HollowWaypointLayerWidget.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "ViewModels/HollowObjectiveModel.h"
#include "ViewModels/HollowVisorContext.h"
#include "Visor/HollowVisorStyle.h"
#include "Visor/HollowWaypointMarkerWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowWaypointLayerWidget)

namespace HollowWaypointLayerPrivate
{
	/** A marker's centre sits on its place. */
	const FVector2D CentreAlignment(0.5, 0.5);
	/** Without a camera manager (tests), the engine's default field of view. */
	constexpr float DefaultFovDegrees = 90.f;
}

void UHollowWaypointLayerWidget::BindContext(UHollowVisorContext& InContext)
{
	Objective = InContext.GetObjective();
	if (UHollowObjectiveModel* Model = Objective.Get())
	{
		ObjectiveHandle = Model->OnObjectiveChanged.AddUObject(this, &ThisClass::Restart);
	}
	Restart();
}

void UHollowWaypointLayerWidget::UnbindContext(UHollowVisorContext& InContext)
{
	if (UHollowObjectiveModel* Model = Objective.Get())
	{
		Model->OnObjectiveChanged.Remove(ObjectiveHandle);
	}
	Objective.Reset();
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimer);
	}
	HideFrom(0);
}

bool UHollowWaypointLayerWidget::IsUpdating() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimerManager().IsTimerActive(UpdateTimer);
}

void UHollowWaypointLayerWidget::Restart()
{
	UWorld* World = GetWorld();
	const UHollowObjectiveModel* Model = Objective.Get();
	const UHollowVisorContext* Owner = GetContext();
	const UHollowVisorStyle* Style = Owner ? Owner->GetStyle() : nullptr;
	if (!World)
	{
		return;
	}
	const bool bAny = Model && Style && !Model->GetObjective().Waypoints.IsEmpty() && !Model->IsScreenOpen();
	if (!bAny)
	{
		World->GetTimerManager().ClearTimer(UpdateTimer);
		HideFrom(0);
		return;
	}
	if (!World->GetTimerManager().IsTimerActive(UpdateTimer))
	{
		World->GetTimerManager().SetTimer(UpdateTimer, this, &ThisClass::UpdateMarkers,
			1.f / FMath::Clamp(Style->WaypointRateHz, UHollowVisorStyle::MinWaypointRateHz, UHollowVisorStyle::MaxWaypointRateHz), /*bLoop=*/true);
	}
	UpdateMarkers();
}

FVector2D UHollowWaypointLayerWidget::GetViewSize() const
{
	if (!TestViewSize.IsZero())
	{
		return TestViewSize;
	}
	return MarkerCanvas->GetCachedGeometry().GetLocalSize();
}

void UHollowWaypointLayerWidget::HideFrom(const int32 FirstHidden)
{
	for (int32 Index = FirstHidden; Index < Markers.Num(); ++Index)
	{
		Markers[Index]->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (FirstHidden == 0)
	{
		Shown.Reset();
	}
}

void UHollowWaypointLayerWidget::UpdateMarkers()
{
	using namespace HollowWaypointLayerPrivate;
	const UHollowObjectiveModel* Model = Objective.Get();
	const UHollowVisorContext* Owner = GetContext();
	const UHollowVisorStyle* Style = Owner ? Owner->GetStyle() : nullptr;
	APlayerController* Player = Owner ? Owner->GetPlayer() : nullptr;
	const FVector2D Size = GetViewSize();
	if (!Model || !Style || !Player || !MarkerClass || Size.X <= 0.0 || Size.Y <= 0.0)
	{
		HideFrom(0);
		return;   // not laid out yet: the next beat places them
	}
	FVector ViewLocation;
	FRotator ViewRotation;
	Player->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const float Fov = Player->PlayerCameraManager ? Player->PlayerCameraManager->GetFOVAngle() : DefaultFovDegrees;

	TArray<FHollowWaypointTarget> Targets;
	Model->GetWaypoints(ViewLocation, Targets);
	Shown.Reset();
	for (const FHollowWaypointTarget& Target : Targets)
	{
		const FHollowWaypointLook* Look = Style->GetWaypointLook(Target.Kind);
		const int32 Metres = HollowWaypointMath::Metres(ViewLocation, Target.Location);
		if (!Look || static_cast<float>(Metres) < Style->WaypointHideMetres)
		{
			continue;
		}
		FVector2D Projected;
		const bool bProjected = HollowWaypointMath::Project(ViewLocation, ViewRotation, Fov, Size, Target.Location, Projected);
		const FVector2D Direction = HollowWaypointMath::ScreenDirection(ViewLocation, ViewRotation, Target.Location);
		const FHollowWaypointPlacement Placement = HollowWaypointMath::Place(Size, Style->WaypointEdgeMargin, bProjected, Projected, Direction);

		const int32 Index = Shown.Num();
		if (!Markers.IsValidIndex(Index))
		{
			UHollowWaypointMarkerWidget* Marker = CreateWidget<UHollowWaypointMarkerWidget>(this, MarkerClass);
			UCanvasPanelSlot* CanvasSlot = MarkerCanvas->AddChildToCanvas(Marker);
			CanvasSlot->SetAutoSize(true);
			CanvasSlot->SetAlignment(CentreAlignment);
			Markers.Add(Marker);
		}
		UHollowWaypointMarkerWidget* Marker = Markers[Index];
		Marker->Show(FText::Format(Style->WaypointFormat, Look->Label, Metres), Look->Color, Placement);
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Marker->Slot))
		{
			CanvasSlot->SetPosition(Placement.Position);
		}
		Shown.Add({Target.Kind, Metres, Placement});
	}
	HideFrom(Shown.Num());
}

FString UHollowWaypointLayerWidget::Describe() const
{
	TArray<FString> Parts;
	for (const FShown& Marker : Shown)
	{
		Parts.Add(FString::Printf(TEXT("%s %d m %s"), *StaticEnum<EHollowWaypointKind>()->GetNameStringByValue(static_cast<int64>(Marker.Kind)), Marker.Metres,
			Marker.Placement.bOnScreen ? TEXT("in view") : *FString::Printf(TEXT("edge %.0f deg"), Marker.Placement.ArrowDegrees)));
	}
	return FString::Printf(TEXT("waypoints %d (%s)%s"), Shown.Num(), *FString::Join(Parts, TEXT(", ")), IsUpdating() ? TEXT("") : TEXT(" idle"));
}
