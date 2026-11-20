#include "Interfaces/HollowLightSource.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowLightSource)

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
