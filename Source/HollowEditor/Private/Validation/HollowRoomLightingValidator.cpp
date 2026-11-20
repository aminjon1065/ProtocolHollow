#include "Validation/HollowRoomLightingValidator.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Data/HollowRoomDefinition.h"
#include "Engine/World.h"
#include "Misc/DataValidation.h"
#include "Rules/HollowLightBudget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowRoomLightingValidator)

#define LOCTEXT_NAMESPACE "HollowRoomLightingValidator"

const UHollowRoomDefinition* UHollowRoomLightingValidator::FindRoomFor(const UWorld& World)
{
	const IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Rooms;
	Registry.GetAssetsByClass(UHollowRoomDefinition::StaticClass()->GetClassPathName(), Rooms, /*bSearchSubClasses=*/true);
	const FName Package = World.GetOutermost()->GetFName();
	for (const FAssetData& Asset : Rooms)
	{
		const UHollowRoomDefinition* Room = Cast<UHollowRoomDefinition>(Asset.GetAsset());
		if (Room && FName(*Room->RoomLevel.GetLongPackageName()) == Package)
		{
			return Room;
		}
	}
	return nullptr;
}

bool UHollowRoomLightingValidator::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const
{
	const UWorld* World = Cast<UWorld>(InObject);
	return World && FindRoomFor(*World);
}

EDataValidationResult UHollowRoomLightingValidator::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context)
{
	const UWorld* World = Cast<UWorld>(InAsset);
	const UHollowRoomDefinition* Room = World ? FindRoomFor(*World) : nullptr;
	if (!Room || !World->PersistentLevel)
	{
		return EDataValidationResult::NotValidated;
	}
	const TArray<FString> Problems = HollowLightBudget::CheckRoomLevel(*Room, *World->PersistentLevel);
	for (const FString& Problem : Problems)
	{
		AssetFails(InAsset, FText::Format(LOCTEXT("Budget", "Light budget of {0}: {1}."), FText::FromString(Room->GetName()), FText::FromString(Problem)));
	}
	if (Problems.IsEmpty())
	{
		AssetPasses(InAsset);
	}
	return Problems.IsEmpty() ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}

#undef LOCTEXT_NAMESPACE
