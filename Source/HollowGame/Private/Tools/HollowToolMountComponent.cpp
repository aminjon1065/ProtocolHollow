#include "Tools/HollowToolMountComponent.h"

#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/HollowToolDefinition.h"
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
	Attach(FirstPersonMesh.Get(), FirstPersonParent, FirstPersonSocket);
	Attach(ThirdPersonMesh.Get(), ThirdPersonParent, ThirdPersonSocket);
}

void UHollowToolMountComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!ShownTool.IsValid() && !DefaultTool.IsNull())
	{
		ShowTool(DefaultTool.LoadSynchronous());   // a small data asset (the tool abilities hold it anyway); its mesh loads asynchronously
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
	if (ShownTool.Get() == Tool && Tool)
	{
		return;
	}
	ShownTool = Tool;
	if (Loading.IsValid())
	{
		Loading->CancelHandle();
		Loading.Reset();
	}
	if (Tool && !Tool->HeldMesh.IsNull() && !Tool->HeldMesh.Get())
	{
		Loading = UAssetManager::GetStreamableManager().RequestAsyncLoad(Tool->HeldMesh.ToSoftObjectPath(), FStreamableDelegate::CreateWeakLambda(this, [this]
		{
			Loading.Reset();
			ApplyMesh();
		}));
		if (Loading.IsValid())
		{
			return;
		}
	}
	ApplyMesh();
}

void UHollowToolMountComponent::ApplyMesh()
{
	const UHollowToolDefinition* Tool = ShownTool.Get();
	UStaticMesh* Mesh = Tool ? Tool->HeldMesh.Get() : nullptr;
	for (UStaticMeshComponent* Component : {FirstPersonMesh.Get(), ThirdPersonMesh.Get()})
	{
		if (Component)
		{
			Component->SetStaticMesh(Mesh);
			Component->SetRelativeTransform(Tool ? Tool->HeldTransform : FTransform::Identity);
			Component->SetVisibility(Mesh != nullptr);
		}
	}
	UE_LOG(LogHollowGame, Verbose, TEXT("%s: holds %s (%s)"), *GetNameSafe(GetOwner()), *GetNameSafe(Tool), *GetNameSafe(Mesh));
}

bool UHollowToolMountComponent::IsFirstPersonView() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn && Pawn->IsLocallyControlled() && Pawn->IsPlayerControlled();
}

bool UHollowToolMountComponent::GetMuzzleLocation(FVector& OutLocation) const
{
	const UHollowToolDefinition* Tool = ShownTool.Get();
	const bool bFirstPerson = IsFirstPersonView();
	const UStaticMeshComponent* Mesh = bFirstPerson ? FirstPersonMesh.Get() : ThirdPersonMesh.Get();
	if (!Tool || !Mesh || !Mesh->GetStaticMesh() || !Mesh->DoesSocketExist(Tool->MuzzleSocket))
	{
		return false;
	}
	OutLocation = Mesh->GetSocketLocation(Tool->MuzzleSocket);
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
