#include "Tools/HollowToolMountComponent.h"

#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/HollowToolDefinition.h"
#include "Data/HollowWeaponDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Pawn.h"
#include "HollowGameLog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowToolMountComponent)

UHollowToolMountComponent::UHollowToolMountComponent()
	: FirstPersonSocket(TEXT("hand_r"))
	, ThirdPersonSocket(TEXT("hand_r"))
{
	PrimaryComponentTick.bCanEverTick = false;
}

UHollowToolMountComponent* UHollowToolMountComponent::Get(const AActor* Owner)
{
	return Owner ? Owner->FindComponentByClass<UHollowToolMountComponent>() : nullptr;
}

void UHollowToolMountComponent::Setup(UStaticMeshComponent* InFirstPersonMesh, UStaticMeshComponent* InThirdPersonMesh, UCameraComponent* InCamera)
{
	FirstPersonMesh = InFirstPersonMesh;
	ThirdPersonMesh = InThirdPersonMesh;
	Camera = InCamera;
	for (UStaticMeshComponent* Mesh : {InFirstPersonMesh, InThirdPersonMesh})
	{
		if (Mesh)
		{
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // looks only: traces, the beam and movement ignore it
			Mesh->SetCanEverAffectNavigation(false);
			Mesh->SetVisibility(false);
		}
	}
	if (InFirstPersonMesh)
	{
		InFirstPersonMesh->SetOnlyOwnerSee(true);
		InFirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
		InFirstPersonMesh->SetCastShadow(false);   // the body's tool casts the owner's shadow
	}
	if (InThirdPersonMesh)
	{
		InThirdPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;   // the owner sees its shadow only
	}
}

void UHollowToolMountComponent::AttachToHands(USceneComponent* FirstPersonParent, USceneComponent* ThirdPersonParent)
{
	const auto Attach = [this](UStaticMeshComponent* Mesh, USceneComponent* Parent, const FName Socket)
	{
		if (!Mesh || !Parent)
		{
			return;
		}
		// Hands without a mesh (the bare class in tests) have no sockets: the tool stays where it is. A mesh without the
		// hand socket is an art problem: said once, and the tool sits at the mesh's origin.
		const USkinnedMeshComponent* Skinned = Cast<USkinnedMeshComponent>(Parent);
		if (Skinned && !Skinned->GetSkinnedAsset())
		{
			return;
		}
		const bool bHasSocket = Parent->DoesSocketExist(Socket);
		UE_CLOG(!bHasSocket, LogHollowGame, Warning, TEXT("%s: %s has no socket %s for the tool in hand."), *GetNameSafe(GetOwner()), *Parent->GetName(), *Socket.ToString());
		Mesh->AttachToComponent(Parent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, bHasSocket ? Socket : NAME_None);
	};
	if (bFirstPersonOnCamera && Camera.IsValid() && FirstPersonMesh.IsValid())
	{
		FirstPersonMesh->AttachToComponent(Camera.Get(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
	else
	{
		Attach(FirstPersonMesh.Get(), FirstPersonParent, FirstPersonSocket);
	}
	Attach(ThirdPersonMesh.Get(), ThirdPersonParent, ThirdPersonSocket);
}

void UHollowToolMountComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!ShownTool.IsValid() && bShowingDefault)
	{
		ShowDefault();
	}
}

void UHollowToolMountComponent::ShowDefault()
{
	// A small data asset (the tool abilities hold it anyway); its mesh loads asynchronously.
	const UHollowToolDefinition* Tool = ClassTool.IsValid() ? ClassTool.Get() : DefaultTool.LoadSynchronous();
	if (Tool)
	{
		Show(Tool);
	}
}

void UHollowToolMountComponent::SetClassTool(const UHollowToolDefinition* Tool)
{
	ClassTool = Tool;
	if (bShowingDefault && Tool && HasBegunPlay())
	{
		ShowDefault();
	}
}

void UHollowToolMountComponent::SetFiring(const bool bFiring)
{
	bFiringShown = bFiring;
	for (UStaticMeshComponent* Component : {FirstPersonMesh.Get(), ThirdPersonMesh.Get()})
	{
		if (Component)
		{
			Component->SetCustomPrimitiveDataFloat(FiringDataIndex, bFiring ? 1.f : 0.f);
		}
	}
}

void UHollowToolMountComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Loading.IsValid())
	{
		Loading->CancelHandle();
		Loading.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void UHollowToolMountComponent::ShowTool(const UHollowToolDefinition* Tool)
{
	bShowingDefault = false;   // a tool was used: it stays in hand
	Show(Tool);
}

void UHollowToolMountComponent::Show(const UHollowToolDefinition* Tool)
{
	if (ShownTool.Get() == Tool && Tool)
	{
		return;
	}
	ShownTool = Tool;
	LoadAndApply();
}

void UHollowToolMountComponent::SetHeldWeapon(const UHollowWeaponDefinition* Weapon)
{
	if (HeldWeapon.Get() == Weapon)
	{
		return;
	}
	HeldWeapon = Weapon;
	LoadAndApply();
}

void UHollowToolMountComponent::GetHeld(TSoftObjectPtr<UStaticMesh>& OutMesh, FTransform& OutTransform, FName& OutMuzzle,
	const FTransform*& OutViewOffset) const
{
	OutViewOffset = nullptr;
	if (const UHollowWeaponDefinition* Weapon = HeldWeapon.Get())
	{
		OutMesh = Weapon->HeldMesh;
		OutTransform = Weapon->HeldTransform;
		OutMuzzle = Weapon->MuzzleSocket;
		OutViewOffset = Weapon->ViewOffset.Equals(FTransform::Identity) ? nullptr : &Weapon->ViewOffset;
		return;
	}
	const UHollowToolDefinition* Tool = ShownTool.Get();
	OutMesh = Tool ? Tool->HeldMesh : TSoftObjectPtr<UStaticMesh>();
	OutTransform = Tool ? Tool->HeldTransform : FTransform::Identity;
	OutMuzzle = Tool ? Tool->MuzzleSocket : NAME_None;
}

void UHollowToolMountComponent::LoadAndApply()
{
	if (Loading.IsValid())
	{
		Loading->CancelHandle();
		Loading.Reset();
	}
	TSoftObjectPtr<UStaticMesh> Mesh;
	FTransform Transform;
	FName Muzzle;
	const FTransform* ViewOffset = nullptr;
	GetHeld(Mesh, Transform, Muzzle, ViewOffset);
	ApplyMesh();   // at once: what is loaded (empty hands while the new mesh loads, never the old one)
	if (!Mesh.IsNull() && !Mesh.Get())
	{
		Loading = UAssetManager::GetStreamableManager().RequestAsyncLoad(Mesh.ToSoftObjectPath(), FStreamableDelegate::CreateWeakLambda(this, [this]
		{
			Loading.Reset();
			ApplyMesh();
		}));
	}
}

void UHollowToolMountComponent::ApplyMesh()
{
	TSoftObjectPtr<UStaticMesh> HeldMesh;
	FTransform HeldTransform;
	FName Muzzle;
	const FTransform* ViewOffset = nullptr;
	GetHeld(HeldMesh, HeldTransform, Muzzle, ViewOffset);
	UStaticMesh* Mesh = HeldMesh.Get();
	for (UStaticMeshComponent* Component : {FirstPersonMesh.Get(), ThirdPersonMesh.Get()})
	{
		if (Component)
		{
			const bool bOnCamera = Component == FirstPersonMesh.Get() && IsFirstPersonOnCamera();
			Component->SetStaticMesh(Mesh);
			if (bOnCamera)
			{
				ViewRest = ViewOffset ? *ViewOffset : FirstPersonViewOffset;
			}
			Component->SetRelativeTransform(bOnCamera ? WithViewFeel(ViewRest) : HeldTransform);
			Component->SetVisibility(Mesh != nullptr);
		}
	}
	UE_LOG(LogHollowGame, Verbose, TEXT("%s: holds %s (%s)"), *GetNameSafe(GetOwner()),
		HeldWeapon.IsValid() ? *GetNameSafe(HeldWeapon.Get()) : *GetNameSafe(ShownTool.Get()), *GetNameSafe(Mesh));
}

void UHollowToolMountComponent::SetViewFeelOffset(const FTransform& Offset)
{
	ViewFeel = Offset;
	if (UStaticMeshComponent* Mesh = FirstPersonMesh.Get(); Mesh && IsFirstPersonOnCamera())
	{
		Mesh->SetRelativeTransform(WithViewFeel(ViewRest));
	}
}

FTransform UHollowToolMountComponent::WithViewFeel(const FTransform& Rest) const
{
	// Turned about the camera's own axes and moved in its space, so a kick is up and back whatever the rest pose.
	return FTransform(ViewFeel.GetRotation() * Rest.GetRotation(), Rest.GetLocation() + ViewFeel.GetLocation(), Rest.GetScale3D());
}

bool UHollowToolMountComponent::IsFirstPersonOnCamera() const
{
	const UStaticMeshComponent* Mesh = FirstPersonMesh.Get();
	return bFirstPersonOnCamera && Mesh && Camera.IsValid() && Mesh->GetAttachParent() == Camera.Get();
}

bool UHollowToolMountComponent::IsFirstPersonView() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn && Pawn->IsLocallyControlled() && Pawn->IsPlayerControlled();
}

bool UHollowToolMountComponent::GetMuzzleLocation(FVector& OutLocation) const
{
	TSoftObjectPtr<UStaticMesh> HeldMesh;
	FTransform HeldTransform;
	FName Muzzle;
	const FTransform* ViewOffset = nullptr;
	GetHeld(HeldMesh, HeldTransform, Muzzle, ViewOffset);
	const bool bFirstPerson = IsFirstPersonView();
	const UStaticMeshComponent* Mesh = bFirstPerson ? FirstPersonMesh.Get() : ThirdPersonMesh.Get();
	if (Muzzle.IsNone() || !Mesh || !Mesh->GetStaticMesh() || !Mesh->DoesSocketExist(Muzzle))
	{
		return false;
	}
	OutLocation = Mesh->GetSocketLocation(Muzzle);
	if (UCameraComponent* View = Camera.Get(); bFirstPerson && View)
	{
		// First-person rendering draws the hands with their own field of view: where the muzzle appears is not where it
		// is. A full-size world effect (the beam) starts where it appears (UE first-person rendering, ignoring its scale).
		FMinimalViewInfo ViewInfo;
		View->GetCameraView(0.f, ViewInfo);
		OutLocation = ViewInfo.TransformWorldToFirstPerson(OutLocation, /*bIgnoreFirstPersonScale=*/true);
	}
	return true;
}
