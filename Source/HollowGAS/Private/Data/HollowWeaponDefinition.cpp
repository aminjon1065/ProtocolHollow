#include "Data/HollowWeaponDefinition.h"

#include "Engine/StaticMesh.h"
#include "GameplayEffect.h"
#include "Weapons/HollowWeaponMath.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowWeaponDefinition)

#define LOCTEXT_NAMESPACE "HollowWeaponDefinition"

const FPrimaryAssetType UHollowWeaponDefinition::AssetType(TEXT("HollowWeapon"));

FPrimaryAssetId UHollowWeaponDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, GetFName());
}

float UHollowWeaponDefinition::GetPelletDamage(const float Distance) const
{
	return HollowWeaponMath::DamageAtDistance(DamagePerPellet, Distance, FalloffStartRange, FalloffEndRange, MinDamageFraction);
}

float UHollowWeaponDefinition::GetSpreadDegrees(const bool bAiming) const
{
	return bAiming ? SpreadDegrees * AimSpreadMultiplier : SpreadDegrees;
}

#if WITH_EDITOR
EDataValidationResult UHollowWeaponDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	const FText Name = FText::FromName(GetFName());
	if (!WeaponTag.IsValid() || !DamageEffect || !AmmoItem.IsValid())
	{
		Context.AddError(FText::Format(LOCTEXT("Incomplete", "{0}: a weapon needs its WeaponTag, DamageEffect and AmmoItem."), Name));
		Result = EDataValidationResult::Invalid;
	}
	if (FalloffEndRange < FalloffStartRange)
	{
		Context.AddError(FText::Format(LOCTEXT("Falloff", "{0}: FalloffEndRange is below FalloffStartRange."), Name));
		Result = EDataValidationResult::Invalid;
	}
	if (MagazineSize < AmmoPerShot)
	{
		Context.AddError(FText::Format(LOCTEXT("Magazine", "{0}: the magazine holds less than one shot."), Name));
		Result = EDataValidationResult::Invalid;
	}
	// A held mesh without the muzzle socket still works (the shots leave from in front of the eyes), so this only warns.
	const UStaticMesh* Mesh = HeldMesh.IsNull() ? nullptr : HeldMesh.LoadSynchronous();
	if (Mesh && !Mesh->FindSocket(MuzzleSocket))
	{
		Context.AddWarning(FText::Format(LOCTEXT("NoMuzzle", "{0}: the held mesh {1} has no socket {2}; the shots leave from in front of the eyes."),
			Name, FText::FromName(Mesh->GetFName()), FText::FromName(MuzzleSocket)));
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
