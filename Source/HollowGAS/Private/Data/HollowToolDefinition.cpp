#include "Data/HollowToolDefinition.h"

#include "Engine/StaticMesh.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowToolDefinition)

#define LOCTEXT_NAMESPACE "HollowToolDefinition"

const FPrimaryAssetType UHollowToolDefinition::AssetType(TEXT("HollowTool"));

FPrimaryAssetId UHollowToolDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, GetFName());
}

#if WITH_EDITOR
EDataValidationResult UHollowToolDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Result = Super::IsDataValid(Context);
	// A held mesh without the muzzle socket still works (the beam falls back to the eyes), so this only warns.
	const UStaticMesh* Mesh = HeldMesh.IsNull() ? nullptr : HeldMesh.LoadSynchronous();
	if (Mesh && !Mesh->FindSocket(MuzzleSocket))
	{
		Context.AddWarning(FText::Format(LOCTEXT("NoMuzzle", "{0}: the held mesh {1} has no socket {2}; the beam leaves from in front of the eyes."),
			FText::FromName(GetFName()), FText::FromName(Mesh->GetFName()), FText::FromName(MuzzleSocket)));
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
