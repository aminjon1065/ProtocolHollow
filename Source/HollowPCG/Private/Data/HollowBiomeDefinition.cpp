#include "Data/HollowBiomeDefinition.h"

#include "Generation/HollowSectorGenerator.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowBiomeDefinition)

const FPrimaryAssetType UHollowBiomeDefinition::AssetType(TEXT("HollowBiome"));

FPrimaryAssetId UHollowBiomeDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, GetFName());
}

TSoftClassPtr<AActor> UHollowBiomeDefinition::FindSocketActor(const FGameplayTag& SocketType) const
{
	const FHollowSocketActorRule* Rule = SocketActors.FindByPredicate([&SocketType](const FHollowSocketActorRule& R) { return R.SocketType == SocketType; });
	return Rule ? Rule->ActorClass : TSoftClassPtr<AActor>();
}

TSoftClassPtr<AActor> UHollowBiomeDefinition::FindConnectionActor(const EHollowConnectorKind Kind) const
{
	const FHollowConnectionActorRule* Rule = ConnectionActors.FindByPredicate([Kind](const FHollowConnectionActorRule& R) { return R.Kind == Kind; });
	return Rule ? Rule->ActorClass : TSoftClassPtr<AActor>();
}

#if WITH_EDITOR
EDataValidationResult UHollowBiomeDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	for (const FString& Error : FHollowSectorGenerator::ValidateRules(GenRules))
	{
		Context.AddError(FText::FromString(Error));
		Result = EDataValidationResult::Invalid;
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif
