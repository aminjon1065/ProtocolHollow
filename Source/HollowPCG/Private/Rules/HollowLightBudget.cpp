#include "Rules/HollowLightBudget.h"

#include "Components/LightComponent.h"
#include "Components/LocalLightComponent.h"
#include "Data/HollowLevelMetrics.h"
#include "Data/HollowRoomDefinition.h"
#include "Engine/Level.h"
#include "GameFramework/Actor.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowLightBudget)

#define LOCTEXT_NAMESPACE "HollowLightBudget"

FHollowLightInfo HollowLightBudget::Describe(const ULightComponent& Light)
{
	FHollowLightInfo Info;
	Info.Name = Light.GetOwner() ? FString::Printf(TEXT("%s.%s"), *Light.GetOwner()->GetActorNameOrLabel(), *Light.GetName()) : Light.GetName();
	Info.bShadowed = Light.CastShadows && Light.Mobility != EComponentMobility::Static;
	Info.bLightFunction = Light.LightFunctionMaterial != nullptr;
	Info.bVolumetricShadow = Info.bShadowed && Light.bCastVolumetricShadow && Light.VolumetricScatteringIntensity > 0.f;
	const ULocalLightComponent* Local = Cast<ULocalLightComponent>(&Light);
	Info.AttenuationRadius = Local ? Local->AttenuationRadius : 0.f;
	return Info;
}

TArray<FHollowLightInfo> HollowLightBudget::CollectLevel(const ULevel& Level)
{
	TArray<FHollowLightInfo> Lights;
	for (const AActor* Actor : Level.Actors)
	{
		if (!Actor)
		{
			continue;
		}
		TInlineComponentArray<ULightComponent*> Components(Actor);
		for (const ULightComponent* Light : Components)
		{
			if (Light && Light->IsVisible())
			{
				Lights.Add(Describe(*Light));
			}
		}
	}
	Lights.Sort([](const FHollowLightInfo& A, const FHollowLightInfo& B) { return A.Name < B.Name; });   // stable reports
	return Lights;
}

TArray<FString> HollowLightBudget::CheckRoom(const TConstArrayView<FHollowLightInfo> Lights, const FHollowRoomLightLimits& Limits, const float MaxRadius)
{
	int32 Shadowed = 0;
	int32 ShadowedFlickering = 0;
	int32 Functions = 0;
	int32 Volumetric = 0;
	TArray<FString> Problems;
	for (const FHollowLightInfo& Light : Lights)
	{
		Shadowed += Light.bShadowed ? 1 : 0;
		ShadowedFlickering += Light.bShadowed && Light.bLightFunction ? 1 : 0;
		Functions += Light.bLightFunction ? 1 : 0;
		Volumetric += Light.bVolumetricShadow ? 1 : 0;
		if (MaxRadius > 0.f && Light.AttenuationRadius > MaxRadius)
		{
			Problems.Add(FString::Printf(TEXT("%s reaches %.0f cm, past the room (%.0f)"), *Light.Name, Light.AttenuationRadius, MaxRadius));
		}
	}
	auto Count = [&Problems](const TCHAR* What, const int32 Value, const int32 Max)
	{
		if (Value > Max)
		{
			Problems.Add(FString::Printf(TEXT("%d %s, at most %d"), Value, What, Max));
		}
	};
	Count(TEXT("lights"), Lights.Num(), Limits.MaxLights);
	Count(TEXT("shadowed lights"), Shadowed, Limits.MaxShadowed);
	Count(TEXT("shadowed flickering lights"), ShadowedFlickering, Limits.MaxShadowedFlickering);
	Count(TEXT("light functions"), Functions, Limits.MaxLightFunctions);
	Count(TEXT("volumetric shadows"), Volumetric, Limits.MaxVolumetricShadows);
	return Problems;
}

int32 HollowLightBudget::WorstShadowedInView(const TConstArrayView<FHollowRoomLightLimits> Rooms)
{
	int32 First = 0;
	int32 Second = 0;
	int32 PassThrough = 0;
	for (const FHollowRoomLightLimits& Room : Rooms)
	{
		if (Room.MaxShadowed > First)
		{
			Second = First;
			First = Room.MaxShadowed;
		}
		else
		{
			Second = FMath::Max(Second, Room.MaxShadowed);
		}
		PassThrough = Room.bPassThrough ? FMath::Max(PassThrough, Room.MaxShadowed) : PassThrough;
	}
	return First + Second + PassThrough;
}

float HollowLightBudget::RoomReach(const UHollowRoomDefinition& Room)
{
	const UHollowLevelMetrics* Metrics = GetDefault<UHollowLevelMetrics>();
	return static_cast<float>(FVector(static_cast<double>(Room.Footprint.X) * Metrics->CellSize, static_cast<double>(Room.Footprint.Y) * Metrics->CellSize,
		static_cast<double>(Room.Footprint.Z) * Metrics->CellHeight).Size());
}

TArray<FString> HollowLightBudget::CheckRoomLevel(const UHollowRoomDefinition& Room, const ULevel& Level)
{
	const UHollowLightBudgetConfig* Budget = UHollowLightBudgetConfig::Get();
	const FHollowRoomLightLimits* Limits = Budget ? Budget->FindLimits(Room.RoomTags) : nullptr;
	if (!Limits)
	{
		return {FString::Printf(TEXT("no light limits for %s (DA_LightBudget, UHollowLevelMetrics::LightBudget)"), *Room.RoomTags.ToStringSimple())};
	}
	return CheckRoom(CollectLevel(Level), *Limits, RoomReach(Room));
}

TArray<FString> HollowLightBudget::CheckHeroRig(const TConstArrayView<FHollowLightInfo> Lights, const bool bArtLocked)
{
	int32 Shadowed = 0;
	int32 Failing = 0;
	for (const FHollowLightInfo& Light : Lights)
	{
		Shadowed += Light.bShadowed ? 1 : 0;
		Failing += Light.bShadowed && Light.bLightFunction ? 1 : 0;
	}
	TArray<FString> Problems;
	if (bArtLocked)
	{
		if (Failing < HeroFailingPanels)
		{
			Problems.Add(FString::Printf(TEXT("needs a failing panel (shadowed, with a light function), has %d"), Failing));
		}
		return Problems;
	}
	if (Shadowed != HeroShadowedPanels)
	{
		Problems.Add(FString::Printf(TEXT("%d shadowed panels, the rig has %d"), Shadowed, HeroShadowedPanels));
	}
	if (Failing != HeroFailingPanels)
	{
		Problems.Add(FString::Printf(TEXT("%d failing panels, the rig has %d"), Failing, HeroFailingPanels));
	}
	return Problems;
}

const UHollowLightBudgetConfig* UHollowLightBudgetConfig::Get()
{
	return GetDefault<UHollowLevelMetrics>()->LightBudget.LoadSynchronous();
}

const FHollowRoomLightLimits* UHollowLightBudgetConfig::FindLimits(const FGameplayTagContainer& RoomTags) const
{
	return Rooms.FindByPredicate([&RoomTags](const FHollowRoomLightLimits& Limits) { return RoomTags.HasTagExact(Limits.RoomType); });
}

#if WITH_EDITOR
EDataValidationResult UHollowLightBudgetConfig::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	const int32 Worst = HollowLightBudget::WorstShadowedInView(Rooms);
	if (Worst > MaxShadowedInView)
	{
		Context.AddError(FText::Format(LOCTEXT("View", "A view could hold {0} shadowed lights, the frame allows {1}."), Worst, MaxShadowedInView));
		Result = EDataValidationResult::Invalid;
	}
	for (const FHollowRoomLightLimits& Limits : Rooms)
	{
		if (Limits.MaxShadowedFlickering > Limits.MaxShadowed || Limits.MaxVolumetricShadows > Limits.MaxShadowed || Limits.MaxShadowed > Limits.MaxLights)
		{
			Context.AddError(FText::Format(LOCTEXT("Limits", "{0}: shadow limits exceed the lights they are part of."), FText::FromName(Limits.RoomType.GetTagName())));
			Result = EDataValidationResult::Invalid;
		}
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
