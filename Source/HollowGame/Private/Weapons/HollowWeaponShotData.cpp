#include "Weapons/HollowWeaponShotData.h"

#include "GameFramework/Actor.h"
#include "UObject/CoreNet.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowWeaponShotData)

FString FHollowWeaponShotData::ToString() const
{
	return FString::Printf(TEXT("shot %u, %d hits"), Serial, Hits.Num());
}

bool FHollowWeaponShotData::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	bool bOk = true;
	bool bPart = true;
	Origin.NetSerialize(Ar, Map, bPart);
	bOk &= bPart;
	Aim.NetSerialize(Ar, Map, bPart);
	bOk &= bPart;
	Ar << Serial;
	uint8 AimBit = bAiming ? 1 : 0;
	Ar.SerializeBits(&AimBit, 1);
	bAiming = AimBit != 0;

	uint8 Count = static_cast<uint8>(FMath::Min(Hits.Num(), MaxHits));
	Ar << Count;
	if (Ar.IsLoading())
	{
		Hits.SetNum(FMath::Min<int32>(Count, MaxHits));
	}
	for (int32 Index = 0; Index < Count && Index < Hits.Num(); ++Index)
	{
		FHollowPelletHit& Hit = Hits[Index];
		UObject* Object = Hit.Actor;
		bOk &= Map && Map->SerializeObject(Ar, AActor::StaticClass(), Object);
		Hit.Actor = Cast<AActor>(Object);
		Hit.Impact.NetSerialize(Ar, Map, bPart);
		bOk &= bPart;
		Hit.Normal.NetSerialize(Ar, Map, bPart);
		bOk &= bPart;
		Ar << Hit.Pellet;
	}
	bOutSuccess = bOk && !Ar.IsError();
	return true;
}
