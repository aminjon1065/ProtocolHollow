#include "Interfaces/HollowLightSource.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowLightSource)

const TCHAR* LexToString(const EHollowLightKind Kind)
{
	switch (Kind)
	{
	case EHollowLightKind::Lamp:    return TEXT("Lamp");
	case EHollowLightKind::Lantern: return TEXT("Lantern");
	}
	return TEXT("?");
}

bool FHollowLightCone::Contains(const FVector& Point) const
{
	const FVector ToPoint = Point - Origin;
	const double Distance = ToPoint.Size();
	if (Distance > Range)
	{
		return false;
	}
	if (Distance <= UE_KINDA_SMALL_NUMBER)
	{
		return true;
	}
	return FVector::DotProduct(ToPoint / Distance, Direction) >= FMath::Cos(FMath::DegreesToRadians(HalfAngleDegrees));
}

void IHollowLightSource::GatherCones(const UWorld* World, TArray<TPair<FHollowLightCone, const AActor*>>& OutCones)
{
	if (!World)
	{
		return;
	}
	TArray<FHollowLightCone> Cones;
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		const IHollowLightSource* Light = Cast<IHollowLightSource>(*It);
		if (!Light)
		{
			continue;
		}
		Cones.Reset();
		Light->GetLightCones(Cones);
		for (const FHollowLightCone& Cone : Cones)
		{
			OutCones.Emplace(Cone, *It);
		}
	}
}
