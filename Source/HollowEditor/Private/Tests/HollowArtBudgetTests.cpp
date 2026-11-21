#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Components/StaticMeshComponent.h"
#include "Data/HollowRoomDefinition.h"
#include "Engine/Level.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Tests/AutomationCommon.h"
#include "UObject/Package.h"
#include "Validation/HollowArtBudget.h"

namespace HollowArtBudgetTests
{
	/** Test data: a narrow texture keeps the source small (its long side is what counts). */
	constexpr int32 Narrow = 16;
	const FString Decor(TEXT("Dressing"));
	const FString Structure(TEXT("Geometry"));

	UTexture2D* MakeTexture(const int32 Size)
	{
		UTexture2D* Texture = NewObject<UTexture2D>(GetTransientPackage());
		Texture->Source.Init(Size, Narrow, /*NumSlices=*/1, /*NumMips=*/1, TSF_BGRA8);
		return Texture;
	}

	AStaticMeshActor* SpawnProp(UWorld* World, const FString& Folder)
	{
		AStaticMeshActor* Prop = World->SpawnActor<AStaticMeshActor>();
		Prop->SetFolderPath(FName(*Folder));
		return Prop;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowArtBudgetTest, "Hollow.Editor.ArtBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowArtBudgetTest::RunTest(const FString& Parameters)
{
	// ADR-0027: the art-pass budget. A texture cooks no larger than the budget (its source, limited by MaxTextureSize and
	// LODBias); decoration in a room level (the decor folders) neither collides nor shapes the navmesh. The rules on
	// made-up assets, then every texture under the scanned paths and every Docks room level: a warning here fails --strict.
	using namespace HollowArtBudgetTests;
	const UHollowArtBudgetSettings* Settings = GetDefault<UHollowArtBudgetSettings>();
	const int32 Budget = Settings->MaxTextureSize;
	if (!TestTrue(TEXT("A texture budget is set (DefaultEditor.ini)"), Budget > 0) || !TestTrue(TEXT("...and the paths it covers"), !Settings->ScannedPaths.IsEmpty())
		|| !TestTrue(TEXT("...and the decor folders"), Settings->DecorFolders.Contains(Decor)))
	{
		return false;
	}

	// Textures.
	UTexture2D* Large = MakeTexture(Budget * 2);
	TestFalse(TEXT("Twice the budget, unlimited: over"), HollowArtBudget::CheckTexture(*Large, Budget).IsEmpty());
	Large->MaxTextureSize = Budget;
	TestTrue(TEXT("...limited by MaxTextureSize: within"), HollowArtBudget::CheckTexture(*Large, Budget).IsEmpty());
	Large->MaxTextureSize = 0;
	Large->LODBias = 1;
	TestTrue(TEXT("...or by one step of LODBias: within"), HollowArtBudget::CheckTexture(*Large, Budget).IsEmpty() && HollowArtBudget::CookedMaxSize(*Large) == Budget);
	TestTrue(TEXT("At the budget: within"), HollowArtBudget::CheckTexture(*MakeTexture(Budget), Budget).IsEmpty());

	// Folders and paths.
	TestTrue(TEXT("A decor folder and its subfolders"), HollowArtBudget::IsDecorFolder(Decor, Settings->DecorFolders)
		&& HollowArtBudget::IsDecorFolder(Decor + TEXT("/Props"), Settings->DecorFolders));
	TestFalse(TEXT("...not structure, nor a folder that only starts alike"), HollowArtBudget::IsDecorFolder(Structure, Settings->DecorFolders)
		|| HollowArtBudget::IsDecorFolder(Decor + TEXT("Old"), Settings->DecorFolders));
	TestTrue(TEXT("Scanned: /Game/Hollow"), HollowArtBudget::IsScanned(TEXT("/Game/Hollow/World/T_Panel"), Settings->ScannedPaths));
	TestFalse(TEXT("...not the engine"), HollowArtBudget::IsScanned(TEXT("/Engine/BasicShapes/Cube"), Settings->ScannedPaths));

	// Decoration in a level.
	FTestWorldWrapper TestWorld;
	if (!TestWorld.CreateTestWorld(EWorldType::Editor))
	{
		TestWorld.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = TestWorld.GetTestWorld();
	AStaticMeshActor* Prop = SpawnProp(World, Decor);
	TestFalse(TEXT("A colliding prop in the decor folder: refused"), HollowArtBudget::CheckDecor(*World->PersistentLevel, Settings->DecorFolders).IsEmpty());
	Prop->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Prop->GetStaticMeshComponent()->SetCanEverAffectNavigation(true);
	TestFalse(TEXT("...without collision but shaping the navmesh: refused"), HollowArtBudget::CheckDecor(*World->PersistentLevel, Settings->DecorFolders).IsEmpty());
	Prop->GetStaticMeshComponent()->SetCanEverAffectNavigation(false);
	SpawnProp(World, Structure);   // a wall: collides and shapes the navmesh, as it should
	TestTrue(TEXT("Looks-only decoration and colliding structure: fine"), HollowArtBudget::CheckDecor(*World->PersistentLevel, Settings->DecorFolders).IsEmpty());

	// The project as it is.
	IAssetRegistry& Registry = IAssetRegistry::GetChecked();
	TArray<FString> Paths = Settings->ScannedPaths;
	Registry.ScanPathsSynchronous(Paths, /*bForceRescan=*/false);
	FARFilter Filter;
	for (const FString& Path : Paths)
	{
		Filter.PackagePaths.Add(FName(*Path));
	}
	Filter.bRecursivePaths = true;
	Filter.bRecursiveClasses = true;
	Filter.ClassPaths.Add(UTexture::StaticClass()->GetClassPathName());
	TArray<FAssetData> Textures;
	Registry.GetAssets(Filter, Textures);
	for (const FAssetData& Asset : Textures)
	{
		if (const UTexture* Texture = Cast<UTexture>(Asset.GetAsset()))
		{
			for (const FString& Problem : HollowArtBudget::CheckTexture(*Texture, Budget))
			{
				AddWarning(FString::Printf(TEXT("Art budget: %s (%s)"), *Problem, *Asset.PackageName.ToString()));
			}
		}
	}
	FARFilter RoomFilter;
	RoomFilter.PackagePaths.Add(FName(TEXT("/HollowSector_Docks")));
	RoomFilter.bRecursivePaths = true;
	RoomFilter.ClassPaths.Add(UHollowRoomDefinition::StaticClass()->GetClassPathName());
	TArray<FAssetData> Rooms;
	Registry.GetAssets(RoomFilter, Rooms);
	for (const FAssetData& Asset : Rooms)
	{
		const UHollowRoomDefinition* Room = Cast<UHollowRoomDefinition>(Asset.GetAsset());
		const UWorld* Level = Room ? Room->RoomLevel.LoadSynchronous() : nullptr;
		if (TestNotNull(*FString::Printf(TEXT("%s level"), *Asset.AssetName.ToString()), Level) && Level->PersistentLevel)
		{
			for (const FString& Problem : HollowArtBudget::CheckDecor(*Level->PersistentLevel, Settings->DecorFolders))
			{
				AddWarning(FString::Printf(TEXT("Art budget, %s: %s"), *Room->GetName(), *Problem));
			}
		}
	}
	AddInfo(FString::Printf(TEXT("%d texture(s) and %d room level(s) checked"), Textures.Num(), Rooms.Num()));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
