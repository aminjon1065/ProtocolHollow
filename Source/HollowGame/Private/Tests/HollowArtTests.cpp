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
#include "Data/HollowWeaponDefinition.h"
#include "Doors/HollowDoor.h"
#include "Doors/HollowDoorStateComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
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
	/** CHAR-01 (create_character_assets.py, ADR-0055): the characters' own meshes, the tool socket, a glow the tint drives. */
	const TCHAR* const FabCharacters = TEXT("/Game/Fab/Paragon");
	const FName ToolSocket(TEXT("ToolMount"));
	const FName GlowParameter(TEXT("DeepEmissiveColor"));
	const TCHAR* const TestPartMesh = TEXT("/Engine/BasicShapes/Cube.Cube");
	const FName TexturePath(TEXT("/Game/Hollow/Art/Textures"));
	const FPrimaryAssetId Engineer(UHollowClassDefinition::AssetType, TEXT("DA_Class_Engineer"));
	const FPrimaryAssetId Occultist(UHollowClassDefinition::AssetType, TEXT("DA_Class_Occultist"));

	/** Budget (create_art_kit.py MAX_TRIANGLES, TEXTURE_MAX_SIZE). */
	constexpr int32 MaxTriangles = 6000;
	constexpr int32 MaxTextureSize = 1024;
	constexpr int32 KitMeshes = 19;   // GP-01: the two weapons and five pickups; GP-02: the Whisperer and its nest; NAR-01b: the datapad;
	                                  // CHAR-01: the 14 armour parts retired (the characters wear their own meshes)
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
	// CHAR-01 (ADR-0055): the characters wear their own Paragon meshes, no part set; the class's colour reaches the
	// ExoFrame's own glow (UHollowCosmeticPartsComponent::TintParameters: a dynamic instance per slot that has it). A part
	// set still puts its parts on the body's bones (Art-01, ADR-0043), seen as the body is (the owner sees only their
	// shadow), no collision or navigation, the tint in their custom primitive data.
	using namespace HollowArtTests;
	FTestWorldWrapper TestWorld;
	UWorld* World = HollowGameTests::CreateWorld(*this, TestWorld);
	AHollowExoFrameCharacter* Character = World ? SpawnPlaying<AHollowExoFrameCharacter>(World, ExoFrameClass) : nullptr;
	UHollowCosmeticPartsComponent* Parts = Character ? Character->GetCosmeticParts() : nullptr;
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TestPartMesh);
	if (!TestNotNull(TEXT("BP_ExoFrame and its parts component"), Parts) || !TestNotNull(TEXT("The engine's cube"), Cube))
	{
		return false;
	}
	TestTrue(TEXT("No parts: its own mesh"), Parts->GetPartComponents().IsEmpty() && Character->GetMesh()->GetSkeletalMeshAsset()
		&& Character->GetMesh()->GetSkeletalMeshAsset()->GetPathName().StartsWith(FabCharacters));
	const FLinearColor Tint(0.2f, 0.9f, 0.4f, 1.f);
	Parts->SetTint(Tint);
	int32 OnBody = 0;
	for (UMaterialInstanceDynamic* Glow : Parts->GetTintedMaterials())
	{
		for (int32 Slot = 0; Slot < Character->GetMesh()->GetNumMaterials(); ++Slot)
		{
			OnBody += Character->GetMesh()->GetMaterial(Slot) == Glow ? 1 : 0;
		}
		FLinearColor Value;
		TestTrue(FString::Printf(TEXT("%s: the class's colour, brighter"), *Glow->GetName()), Glow->GetVectorParameterValue(FHashedMaterialParameterInfo(GlowParameter), Value)
			&& Value.G > 0.f && FMath::IsNearlyEqual(Value.R / Value.G, Tint.R / Tint.G, 0.01f) && FMath::IsNearlyEqual(Value.B / Value.G, Tint.B / Tint.G, 0.01f));
	}
	TestTrue(FString::Printf(TEXT("The tint glows on the body (%d slots)"), OnBody), OnBody > 0 && OnBody == Parts->GetTintedMaterials().Num());

	// A part set (Art-01): a cube on the body's head, another on its right hand.
	UHollowCosmeticPartSet* Set = NewObject<UHollowCosmeticPartSet>(GetTransientPackage());
	FHollowCosmeticPart Helmet;
	Helmet.Mesh = Cube;
	Helmet.Bone = TEXT("head");
	FHollowCosmeticPart Glove = Helmet;
	Glove.Bone = TEXT("hand_r");
	Set->Parts = {Helmet, Glove};
	Parts->ApplySet(Set);   // its mesh is in memory: attached at once
	for (const UStaticMeshComponent* Part : Parts->GetPartComponents())
	{
		TestTrue(TEXT("A part: on the body's bone"), Part->GetAttachParent() == Character->GetMesh()
			&& Character->GetMesh()->DoesSocketExist(Part->GetAttachSocketName()));
		TestTrue(TEXT("...no collision, no navigation"), Part->GetCollisionEnabled() == ECollisionEnabled::NoCollision && !Part->CanEverAffectNavigation());
		TestTrue(TEXT("...seen as the body is"), Part->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::WorldSpaceRepresentation);
		const TArray<float>& Data = Part->GetCustomPrimitiveData().Data;
		TestTrue(TEXT("...the tint in its custom primitive data (0..3)"), Data.Num() >= 4 && FMath::IsNearlyEqual(Data[0], Tint.R)
			&& FMath::IsNearlyEqual(Data[1], Tint.G) && FMath::IsNearlyEqual(Data[2], Tint.B));
	}
	TestEqual(TEXT("Two parts"), Parts->GetPartComponents().Num(), 2);
	Parts->ApplySet(nullptr);
	TestTrue(TEXT("...taken off"), Parts->GetPartComponents().IsEmpty());

	// The enemies: their own meshes and materials, no part set.
	for (const TCHAR* Path : {PilgrimClass, StalkerClass})
	{
		const UClass* Class = LoadClass<ACharacter>(nullptr, Path);
		const ACharacter* Enemy = Class ? Class->GetDefaultObject<ACharacter>() : nullptr;
		const USkeletalMeshComponent* Body = Enemy ? Enemy->GetMesh() : nullptr;
		const USkeletalMesh* Mesh = Body ? Body->GetSkeletalMeshAsset() : nullptr;
		const UMaterialInterface* First = Mesh && !Mesh->GetMaterials().IsEmpty() ? Mesh->GetMaterials()[0].MaterialInterface.Get() : nullptr;
		TestTrue(FString::Printf(TEXT("%s: its own Paragon mesh and materials, none of the kit over them (%s)"), Path, *GetPathNameSafe(First)), Mesh
			&& Mesh->GetPathName().StartsWith(FabCharacters) && First && First->GetPathName().StartsWith(FabCharacters) && Body->OverrideMaterials.IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowArtToolInHandTest, "Hollow.Game.Art.ToolInHand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowArtToolInHandTest::RunTest(const FString& Parameters)
{
	// Art-01: the owner's tool rides the camera (no first-person arms hold it: CHAR-01, AK-4); CHAR-01 (ADR-0055): the
	// body holds it in its ToolMount socket (where Wraith's rifle sat). The class's tool is in hand until another is used;
	// a running tool glows (custom primitive data 0).
	using namespace HollowArtTests;
	FTestWorldWrapper TestWorld;
	UWorld* World = HollowGameTests::CreateWorld(*this, TestWorld);
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
	TestTrue(TEXT("...the body's in its ToolMount"), Mount->GetThirdPersonMesh()->GetAttachParent() == Character->GetMesh()
		&& Mount->GetThirdPersonMesh()->GetAttachSocketName() == ToolSocket && Character->GetMesh()->DoesSocketExist(ToolSocket));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowArtWeaponInHandTest, "Hollow.Game.Art.WeaponInHand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowArtWeaponInHandTest::RunTest(const FString& Parameters)
{
	// GP-01 (ADR-0044): a weapon in hand replaces the tool (its mesh, muzzle and view offset) and gives it back; its shot
	// effects are Niagara systems (a test world has no FX scene to spawn them in: the cue stops each burst on a timer).
	using namespace HollowArtTests;
	FTestWorldWrapper TestWorld;
	UWorld* World = HollowGameTests::CreateWorld(*this, TestWorld);
	AHollowExoFrameCharacter* Character = World ? SpawnPlaying<AHollowExoFrameCharacter>(World, ExoFrameClass) : nullptr;
	UHollowToolMountComponent* Mount = Character ? UHollowToolMountComponent::Get(Character) : nullptr;
	const UHollowWeaponDefinition* Weapons[] = {
		LoadObject<UHollowWeaponDefinition>(nullptr, TEXT("/Game/Hollow/Weapons/Nailgun/DA_Weapon_Nailgun.DA_Weapon_Nailgun")),
		LoadObject<UHollowWeaponDefinition>(nullptr, TEXT("/Game/Hollow/Weapons/Shotgun/DA_Weapon_Shotgun.DA_Weapon_Shotgun"))};
	if (!TestNotNull(TEXT("BP_ExoFrame's tool mount"), Mount) || !TestTrue(TEXT("The two weapons"), Weapons[0] && Weapons[1]))
	{
		return false;
	}
	const UHollowToolDefinition* Tool = Mount->GetShownTool();
	if (Tool)
	{
		Tool->HeldMesh.LoadSynchronous();   // the tool comes back at once (an unloaded one shows empty hands while it loads)
	}
	for (const UHollowWeaponDefinition* Weapon : Weapons)
	{
		UStaticMesh* Mesh = Weapon->HeldMesh.LoadSynchronous();
		TestTrue(Weapon->GetName() + TEXT(": a held mesh with its muzzle"), Mesh && Mesh->FindSocket(Weapon->MuzzleSocket) != nullptr);
		Mount->SetHeldWeapon(Weapon);
		TestTrue(Weapon->GetName() + TEXT(": in both hands"), Mount->GetFirstPersonMesh()->GetStaticMesh() == Mesh && Mount->GetThirdPersonMesh()->GetStaticMesh() == Mesh);
		TestTrue(Weapon->GetName() + TEXT(": the first-person one at its view offset"),
			Mount->GetFirstPersonMesh()->GetRelativeTransform().Equals(Weapon->ViewOffset, 0.01));
		FVector Muzzle;
		TestTrue(Weapon->GetName() + TEXT(": a muzzle to shoot from"), Mount->GetMuzzleLocation(Muzzle));
		for (const TSoftObjectPtr<UFXSystemAsset>* Effect : {&Weapon->MuzzleFlash, &Weapon->Tracer, &Weapon->Impact})
		{
			TestNotNull(Weapon->GetName() + TEXT(": a Niagara shot effect"), Cast<UNiagaraSystem>(Effect->LoadSynchronous()));
		}
	}
	Mount->SetHeldWeapon(nullptr);
	TestTrue(TEXT("Put away: the tool again"), Mount->GetHeldWeapon() == nullptr && Mount->GetShownTool() == Tool
		&& (!Tool || Mount->GetFirstPersonMesh()->GetStaticMesh() == Tool->HeldMesh.Get()));
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
