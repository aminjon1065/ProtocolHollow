#include "Tests/HollowGameTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/AssetRegistryModule.h"
#include "Camera/CameraComponent.h"
#include "Carry/HollowAetherCore.h"
#include "Classes/HollowClassDefinition.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Cosmetics/HollowCosmeticParts.h"
#include "Data/HollowLanternDefinition.h"
#include "Data/HollowToolDefinition.h"
#include "Doors/HollowDoor.h"
#include "Doors/HollowDoorStateComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "Settings/HollowGameUserSettings.h"
#include "Tags/HollowGameplayTags.h"
#include "Tools/HollowLanternCue.h"
#include "Tools/HollowToolBeamCue.h"
#include "Tools/HollowToolMountComponent.h"
#include "UObject/UObjectGlobals.h"

namespace HollowArtTests
{
	// Content under test (create_art_kit.py, ADR-0043).
	const TCHAR* const ExoFrameClass = TEXT("/Game/Hollow/Characters/ExoFrame/BP_ExoFrame.BP_ExoFrame_C");
	const TCHAR* const PilgrimClass = TEXT("/Game/Hollow/Characters/Enemies/IronPilgrim/BP_IronPilgrim.BP_IronPilgrim_C");
	const TCHAR* const StalkerClass = TEXT("/Game/Hollow/Characters/Enemies/EchoStalker/BP_EchoStalker.BP_EchoStalker_C");
	const TCHAR* const DoorClass = TEXT("/Game/Hollow/World/Kit/Doors/BP_Door.BP_Door_C");
	const TCHAR* const CoreClass = TEXT("/Game/Hollow/Items/AetherCore/BP_AetherCore.BP_AetherCore_C");
	const TCHAR* const BeamCueClass = TEXT("/Game/Hollow/GAS/Cues/GCN_Tool_Beam.GCN_Tool_Beam_C");
	const TCHAR* const LanternCueClass = TEXT("/Game/Hollow/GAS/Cues/GCN_Tool_Lantern.GCN_Tool_Lantern_C");
	const TCHAR* const CutterPath = TEXT("/Game/Hollow/Tools/PlasmaCutter/DA_Tool_PlasmaCutter.DA_Tool_PlasmaCutter");
	const TCHAR* const WelderPath = TEXT("/Game/Hollow/Tools/Welder/DA_Tool_Welder.DA_Tool_Welder");
	const TCHAR* const LanternPath = TEXT("/Game/Hollow/Tools/AetherLantern/DA_Tool_AetherLantern.DA_Tool_AetherLantern");
	const TCHAR* const CoreLook = TEXT("/Game/Hollow/Items/AetherCore/MI_AetherCore.MI_AetherCore");
	const FName KitPath(TEXT("/Game/Hollow/Art/Kit"));
	const FName TexturePath(TEXT("/Game/Hollow/Art/Textures"));
	const FPrimaryAssetId Engineer(UHollowClassDefinition::AssetType, TEXT("DA_Class_Engineer"));
	const FPrimaryAssetId Occultist(UHollowClassDefinition::AssetType, TEXT("DA_Class_Occultist"));

	/** Budget (create_art_kit.py MAX_TRIANGLES, TEXTURE_MAX_SIZE). */
	constexpr int32 MaxTriangles = 6000;
	constexpr int32 MaxTextureSize = 1024;
	constexpr int32 KitMeshes = 23;
	constexpr int32 CcTextures = 18;
	/** Test data. */
	constexpr float TickSeconds = 1.f / 30.f;
	constexpr int32 LoadFrames = 30;
	constexpr float WeldWork = 0.5f;
	constexpr float IntensityTolerance = 0.01f;   // of the steady brightness
	constexpr int32 SettleSlackFrames = 2;

	void Tick(FTestWorldWrapper& World, const int32 Frames)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			++GFrameCounter;
			World.TickTestWorld(TickSeconds);
		}
	}

	/** Waits for the parts' asynchronous load (frames tick the streamable manager's callbacks). */
	bool WaitForParts(FTestWorldWrapper& World, const UHollowCosmeticPartsComponent& Parts)
	{
		for (int32 Frame = 0; Frame < LoadFrames && (Parts.IsLoading() || Parts.GetPartComponents().IsEmpty()); ++Frame)
		{
			FlushAsyncLoading();
			Tick(World, 1);
		}
		return !Parts.IsLoading() && !Parts.GetPartComponents().IsEmpty();
	}

	/** Loads the kit's meshes and materials the characters' part sets use. */
	void PreloadParts()
	{
		for (const TCHAR* Name : {TEXT("DA_Parts_ExoFrame"), TEXT("DA_Parts_IronPilgrim"), TEXT("DA_Parts_EchoStalker")})
		{
			const UHollowCosmeticPartSet* Set = LoadObject<UHollowCosmeticPartSet>(nullptr, *FString::Printf(TEXT("/Game/Hollow/Art/Parts/%s.%s"), Name, Name));
			for (const FHollowCosmeticPart& Part : Set ? Set->Parts : TArray<FHollowCosmeticPart>())
			{
				Part.Mesh.LoadSynchronous();
				for (const TSoftObjectPtr<UMaterialInterface>& Material : Part.Materials)
				{
					Material.LoadSynchronous();
				}
			}
		}
	}

	template <typename TActor>
	TActor* SpawnPlaying(UWorld* World, const TCHAR* ClassPath)
	{
		const TSubclassOf<TActor> Class = LoadClass<TActor>(nullptr, ClassPath);
		TActor* Actor = Class ? World->SpawnActor<TActor>(Class, FTransform::Identity) : nullptr;
		if (Actor && !Actor->HasActorBegunPlay())
		{
			Actor->DispatchBeginPlay();
		}
		return Actor;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowArtCosmeticPartsTest, "Hollow.Game.Art.CosmeticParts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowArtCosmeticPartsTest::RunTest(const FString& Parameters)
{
	// Art-01 (ADR-0043): BP_ExoFrame wears DA_Parts_ExoFrame on the placeholder skeleton: body parts on the body's bones,
	// seen as the body is (the owner sees only their shadow), bracers on the first-person hands only the owner sees;
	// no part collides or touches navigation; a tint reaches every part. The enemies wear their own sets.
	using namespace HollowArtTests;
	FTestWorldWrapper TestWorld;
	UWorld* World = HollowGameTests::CreateWorld(*this, TestWorld);
	PreloadParts();   // the parts attach at BeginPlay when their meshes are in memory (else a frame later)
	AHollowExoFrameCharacter* Character = World ? SpawnPlaying<AHollowExoFrameCharacter>(World, ExoFrameClass) : nullptr;
	UHollowCosmeticPartsComponent* Parts = Character ? Character->GetCosmeticParts() : nullptr;
	if (!TestNotNull(TEXT("BP_ExoFrame and its parts"), Parts) || !TestTrue(TEXT("...loaded and attached"), WaitForParts(TestWorld, *Parts)))
	{
		return false;
	}
	int32 FirstPerson = 0;
	for (const UStaticMeshComponent* Part : Parts->GetPartComponents())
	{
		const bool bOnHands = Part->GetAttachParent() == Character->GetFirstPersonMesh();
		FirstPerson += bOnHands ? 1 : 0;
		TestTrue(FString::Printf(TEXT("%s: on a mesh's bone"), *GetNameSafe(Part->GetStaticMesh())), (bOnHands || Part->GetAttachParent() == Character->GetMesh())
			&& !Part->GetAttachSocketName().IsNone() && Cast<USkinnedMeshComponent>(Part->GetAttachParent())->DoesSocketExist(Part->GetAttachSocketName()));
		TestTrue(TEXT("...no collision, no navigation"), Part->GetCollisionEnabled() == ECollisionEnabled::NoCollision && !Part->CanEverAffectNavigation());
		TestTrue(TEXT("...seen as its mesh is"), bOnHands ? Part->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::FirstPerson && Part->bOnlyOwnerSee
			: Part->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::WorldSpaceRepresentation);
	}
	TestTrue(FString::Printf(TEXT("Helmet, chest, pack, pauldrons, bracers, shins (%d), two bracers on the hands"), Parts->GetPartComponents().Num()),
		Parts->GetPartComponents().Num() >= 9 && FirstPerson == 2);
	const FLinearColor Tint(0.2f, 0.9f, 0.4f, 1.f);
	Parts->SetTint(Tint);
	const TArray<float>& Data = Parts->GetPartComponents()[0]->GetCustomPrimitiveData().Data;
	TestTrue(TEXT("The tint reaches the parts (custom primitive data 0..3)"), Data.Num() >= 4 && FMath::IsNearlyEqual(Data[0], Tint.R)
		&& FMath::IsNearlyEqual(Data[1], Tint.G) && FMath::IsNearlyEqual(Data[2], Tint.B));

	// The enemies: their own sets, their bodies out of the placeholder's materials.
	for (const TCHAR* Path : {PilgrimClass, StalkerClass})
	{
		const UClass* Class = LoadClass<ACharacter>(nullptr, Path);
		const ACharacter* Enemy = Class ? Class->GetDefaultObject<ACharacter>() : nullptr;
		const UHollowCosmeticPartsComponent* EnemyParts = Enemy ? UHollowCosmeticPartsComponent::Get(Enemy) : nullptr;
		TestTrue(FString::Printf(TEXT("%s: a part set and a body material of the kit"), Path), EnemyParts && Enemy->GetMesh()->GetMaterial(0)
			&& Enemy->GetMesh()->GetMaterial(0)->GetPathName().StartsWith(TEXT("/Game/Hollow/Art/")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowArtToolInHandTest, "Hollow.Game.Art.ToolInHand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowArtToolInHandTest::RunTest(const FString& Parameters)
{
	// Art-01: the owner's tool rides the camera (the placeholder arms hold nothing up), the body holds it in its hand;
	// the class's tool is in hand until another is used; a running tool glows (custom primitive data 0).
	using namespace HollowArtTests;
	FTestWorldWrapper TestWorld;
	UWorld* World = HollowGameTests::CreateWorld(*this, TestWorld);
	PreloadParts();
	AHollowExoFrameCharacter* Character = World ? SpawnPlaying<AHollowExoFrameCharacter>(World, ExoFrameClass) : nullptr;
	UHollowToolMountComponent* Mount = Character ? UHollowToolMountComponent::Get(Character) : nullptr;
	const UHollowToolDefinition* Cutter = LoadObject<UHollowToolDefinition>(nullptr, CutterPath);
	const UHollowToolDefinition* Welder = LoadObject<UHollowToolDefinition>(nullptr, WelderPath);
	const UHollowToolDefinition* Lantern = LoadObject<UHollowToolDefinition>(nullptr, LanternPath);
	if (!TestNotNull(TEXT("BP_ExoFrame's tool mount"), Mount) || !TestTrue(TEXT("The three tools"), Cutter && Welder && Lantern))
	{
		return false;
	}
	for (const UHollowToolDefinition* Tool : {Cutter, Welder, Lantern})
	{
		const UStaticMesh* Mesh = Tool->HeldMesh.LoadSynchronous();
		TestTrue(FString::Printf(TEXT("%s: a held mesh with its muzzle"), *Tool->GetName()), Mesh && Mesh->FindSocket(Tool->MuzzleSocket) != nullptr);
	}
	TestTrue(TEXT("The owner's tool rides the camera"), Mount->GetFirstPersonMesh()->GetAttachParent() == Character->GetFirstPersonCamera());
	TestTrue(TEXT("...the body's in its hand"), Mount->GetThirdPersonMesh()->GetAttachParent() == Character->GetMesh());
	TestTrue(TEXT("The cutter by default"), Mount->GetShownTool() == Cutter);
	Mount->SetClassTool(Lantern);
	TestTrue(TEXT("An Occultist holds the lantern"), Mount->GetShownTool() == Lantern);
	Mount->ShowTool(Welder);
	Mount->SetClassTool(Cutter);
	TestTrue(TEXT("...until a tool is used: it stays in hand"), Mount->GetShownTool() == Welder);
	Mount->SetFiring(true);
	const TArray<float>& Firing = Mount->GetFirstPersonMesh()->GetCustomPrimitiveData().Data;
	TestTrue(TEXT("Running: it glows"), Mount->IsFiring() && Firing.IsValidIndex(UHollowToolMountComponent::FiringDataIndex)
		&& Firing[UHollowToolMountComponent::FiringDataIndex] == 1.f);
	Mount->SetFiring(false);
	TestTrue(TEXT("...and stops"), Mount->GetFirstPersonMesh()->GetCustomPrimitiveData().Data[UHollowToolMountComponent::FiringDataIndex] == 0.f);
	const UHollowClassDefinition* EngineerClass = UHollowClassDefinition::Load(Engineer);
	const UHollowClassDefinition* OccultistClass = UHollowClassDefinition::Load(Occultist);
	TestTrue(TEXT("The classes' tools: the cutter, the lantern"), EngineerClass && OccultistClass && EngineerClass->HeldTool.LoadSynchronous() == Cutter
		&& OccultistClass->HeldTool.LoadSynchronous() == Lantern);
	const UClass* Cue = LoadClass<AHollowToolBeamCue>(nullptr, BeamCueClass);
	const UNiagaraComponent* Sparks = Cue ? Cue->GetDefaultObject<AHollowToolBeamCue>()->FindComponentByClass<UNiagaraComponent>() : nullptr;
	TestTrue(TEXT("The beam sparks (NS_Tool_BeamSparks)"), Sparks && Sparks->GetAsset() && !Sparks->bAutoActivate);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowArtLanternFlashTest, "Hollow.Game.Art.LanternFlash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowArtLanternFlashTest::RunTest(const FString& Parameters)
{
	// AK-8: the Aether lantern flares as it lights and settles to its steady cone over FlashSeconds (its refresh timer,
	// no Tick); none with reduced flashes (H-1); its flame glows while it runs and dims when it goes out.
	using namespace HollowArtTests;
	FTestWorldWrapper TestWorld;
	UWorld* World = HollowGameTests::CreateWorld(*this, TestWorld);
	PreloadParts();
	AHollowExoFrameCharacter* Character = World ? SpawnPlaying<AHollowExoFrameCharacter>(World, ExoFrameClass) : nullptr;
	UHollowToolMountComponent* Mount = Character ? UHollowToolMountComponent::Get(Character) : nullptr;
	const UHollowLanternDefinition* Lantern = LoadObject<UHollowLanternDefinition>(nullptr, LanternPath);
	const TSubclassOf<AHollowLanternCue> CueClass = LoadClass<AHollowLanternCue>(nullptr, LanternCueClass);
	UHollowGameUserSettings* Settings = UHollowGameUserSettings::Get();
	if (!TestNotNull(TEXT("BP_ExoFrame's tool mount"), Mount) || !TestNotNull(TEXT("DA_Tool_AetherLantern"), Lantern)
		|| !TestNotNull(TEXT("GCN_Tool_Lantern"), CueClass.Get()) || !TestNotNull(TEXT("The game's settings class"), Settings))
	{
		return false;
	}
	const AHollowLanternCue* Defaults = CueClass->GetDefaultObject<AHollowLanternCue>();
	const float Steady = Defaults->GetLight()->Intensity;
	const float Flare = Defaults->GetFlashIntensityScale();
	TestTrue(FString::Printf(TEXT("GCN_Tool_Lantern flares x%.1f over %.2f s"), Flare, Defaults->GetFlashSeconds()), Steady > 0.f && Flare > 1.f
		&& Defaults->GetFlashSeconds() > 0.f);
	FGameplayCueParameters Cue;
	Cue.SourceObject = Lantern;
	// The cues stay in the world (the cue manager recycles them; destroying one is an error) and go with it.
	auto Light = [World, &CueClass, Character, &Cue]()
	{
		AHollowLanternCue* Spawned = World->SpawnActor<AHollowLanternCue>(CueClass, FTransform::Identity);
		if (Spawned)
		{
			Spawned->OnActive(Character, Cue);
		}
		return Spawned;
	};
	auto Near = [Steady](const float Intensity, const float Scale) { return FMath::IsNearlyEqual(Intensity, Steady * Scale, Steady * IntensityTolerance); };
	const bool bReducedBefore = Settings->IsReducedFlashes();

	Settings->SetReducedFlashes(false, /*bSave=*/false);
	AHollowLanternCue* Lit = Light();
	if (TestNotNull(TEXT("The light cue"), Lit))
	{
		TestTrue(FString::Printf(TEXT("Lit: it flares (%.0f)"), Lit->GetLight()->Intensity), Near(Lit->GetLight()->Intensity, Flare));
		TestTrue(TEXT("...its flame glows"), Mount->IsFiring() && Mount->GetShownTool() == Lantern);
		Tick(TestWorld, FMath::CeilToInt32(Lit->GetFlashSeconds() / TickSeconds) + SettleSlackFrames);
		TestTrue(FString::Printf(TEXT("...and settles to its steady cone (%.0f)"), Lit->GetLight()->Intensity), Near(Lit->GetLight()->Intensity, 1.f));
		Lit->OnRemove(Character, Cue);
		TestTrue(TEXT("Out: the light hides, the flame dims"), !Lit->GetLight()->IsVisible() && !Mount->IsFiring());
	}
	Settings->SetReducedFlashes(true, /*bSave=*/false);
	AHollowLanternCue* Calm = Light();
	if (TestNotNull(TEXT("The light cue again"), Calm))
	{
		TestTrue(FString::Printf(TEXT("Reduced flashes: steady from the start (%.0f)"), Calm->GetLight()->Intensity), Near(Calm->GetLight()->Intensity, 1.f));
		Calm->OnRemove(Character, Cue);
	}
	Settings->SetReducedFlashes(bReducedBefore, /*bSave=*/false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowArtDoorSeamTest, "Hollow.Game.Art.DoorSeam",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowArtDoorSeamTest::RunTest(const FString& Parameters)
{
	// Art-01: BP_Door's leaf has its weld seam: a fresh weld heats it now (HeatTime = this world's time, the material
	// cools it by itself) and the seam shows how welded the door is; a door seen already welded is cold.
	using namespace HollowArtTests;
	FTestWorldWrapper TestWorld;
	UWorld* World = HollowGameTests::CreateWorld(*this, TestWorld);
	AHollowDoor* Door = World ? SpawnPlaying<AHollowDoor>(World, DoorClass) : nullptr;
	UMaterialInstanceDynamic* Seam = Door ? Door->GetSeamMaterial() : nullptr;
	if (!TestNotNull(TEXT("BP_Door's seam material"), Seam))
	{
		return false;
	}
	float HeatBefore = 0.f;
	Seam->GetScalarParameterValue(FName(TEXT("HeatTime")), HeatBefore);
	TestTrue(TEXT("Cold at first"), HeatBefore < 0.f);
	Tick(TestWorld, LoadFrames);
	const TOptional<EHollowDoorEvent> Weld = Door->GetDoorState()->ToolEvent(HollowTags::Ability_Tool_Welder);
	if (!TestTrue(TEXT("The welder works on doors"), Weld.IsSet()))
	{
		return false;
	}
	Door->GetDoorState()->ApplyTool(Weld.GetValue(), WeldWork);
	float Heat = 0.f;
	float Welded = 0.f;
	Seam->GetScalarParameterValue(FName(TEXT("HeatTime")), Heat);
	Seam->GetScalarParameterValue(FName(TEXT("Weld")), Welded);
	TestTrue(FString::Printf(TEXT("A fresh weld heats the seam now (%.2f at %.2f)"), Heat, World->GetTimeSeconds()), FMath::IsNearlyEqual(Heat, World->GetTimeSeconds(), 0.01f));
	TestTrue(FString::Printf(TEXT("...and it shows the weld (%.2f)"), Welded), Welded > 0.f && FMath::IsNearlyEqual(Welded, Door->GetDoorState()->GetWeld(), 0.01f));
	return true;
}

#if WITH_EDITOR   // source-side data: the texture limits and Nanite flags exist only in the editor
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowArtKitDataTest, "Hollow.Game.Art.Kit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowArtKitDataTest::RunTest(const FString& Parameters)
{
	// Art-01: the kit stays in budget (triangles, no Nanite, every slot dressed), its CC0 textures at most 1K, and the
	// Aether core shows the container with the item's look on its crystal.
	using namespace HollowArtTests;
	const IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Meshes;
	Registry.GetAssetsByPath(KitPath, Meshes, /*bRecursive=*/true);
	int32 Checked = 0;
	for (const FAssetData& Asset : Meshes)
	{
		const UStaticMesh* Mesh = Cast<UStaticMesh>(Asset.GetAsset());
		if (!Mesh)
		{
			continue;
		}
		++Checked;
		bool bDressed = true;
		for (const FStaticMaterial& Material : Mesh->GetStaticMaterials())
		{
			bDressed &= Material.MaterialInterface != nullptr;
		}
		const int32 Triangles = Mesh->GetRenderData() && Mesh->GetRenderData()->LODResources.Num() > 0 ? Mesh->GetRenderData()->LODResources[0].GetNumTriangles() : 0;
		TestTrue(FString::Printf(TEXT("%s: %d triangles (at most %d), every slot dressed, no Nanite"), *Mesh->GetName(), Triangles, MaxTriangles),
			Triangles > 0 && Triangles <= MaxTriangles && bDressed && !Mesh->IsNaniteEnabled());
	}
	TestEqual(TEXT("The kit's meshes"), Checked, KitMeshes);
	TArray<FAssetData> Textures;
	Registry.GetAssetsByPath(TexturePath, Textures, /*bRecursive=*/true);
	int32 TextureCount = 0;
	for (const FAssetData& Asset : Textures)
	{
		if (const UTexture2D* Texture = Cast<UTexture2D>(Asset.GetAsset()))
		{
			++TextureCount;
			TestTrue(FString::Printf(TEXT("%s: cooked at most %d"), *Texture->GetName(), MaxTextureSize),
				Texture->MaxTextureSize > 0 && Texture->MaxTextureSize <= MaxTextureSize);
		}
	}
	TestEqual(TEXT("Six CC0 sets, three maps each"), TextureCount, CcTextures);
	const UClass* Core = LoadClass<AHollowAetherCore>(nullptr, CoreClass);
	const UStaticMeshComponent* Visual = Core ? Core->GetDefaultObject<AHollowAetherCore>()->GetVisual() : nullptr;
	TestTrue(TEXT("BP_AetherCore: the container, the item's look on the crystal"), Visual && Visual->GetStaticMesh()
		&& Visual->GetStaticMesh()->GetName() == TEXT("SM_AetherCore") && Visual->GetMaterial(0) == LoadObject<UMaterialInterface>(nullptr, CoreLook));
	return true;
}
#endif

#endif
