#include "Cosmetics/HollowCosmeticParts.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowCosmeticParts)

#define LOCTEXT_NAMESPACE "HollowCosmeticParts"

#if WITH_EDITOR
EDataValidationResult UHollowCosmeticPartSet::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	for (int32 Index = 0; Index < Parts.Num(); ++Index)
	{
		if (Parts[Index].Mesh.IsNull() || Parts[Index].Bone.IsNone())
		{
			Context.AddError(FText::Format(LOCTEXT("Part", "{0}: part {1} needs a mesh and a bone."), FText::FromName(GetFName()), Index));
			Result = EDataValidationResult::Invalid;
		}
	}
	return Result;
}
#endif

UHollowCosmeticPartsComponent::UHollowCosmeticPartsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UHollowCosmeticPartsComponent* UHollowCosmeticPartsComponent::Get(const AActor* Owner)
{
	return Owner ? Owner->FindComponentByClass<UHollowCosmeticPartsComponent>() : nullptr;
}

void UHollowCosmeticPartsComponent::Setup(USkeletalMeshComponent* InBody, USkeletalMeshComponent* InFirstPerson)
{
	Body = InBody;
	FirstPerson = InFirstPerson;
}

USkeletalMeshComponent* UHollowCosmeticPartsComponent::FindBody() const
{
	if (Body.IsValid())
	{
		return Body.Get();
	}
	return GetOwner() ? GetOwner()->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
}

bool UHollowCosmeticPartsComponent::IsLoading() const
{
	return Loading.IsValid() && Loading->IsLoadingInProgress();
}

void UHollowCosmeticPartsComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!PartSet.IsNull())
	{
		ApplySet(PartSet.LoadSynchronous());   // the set itself is a few names; its meshes and materials load asynchronously
	}
}

void UHollowCosmeticPartsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Clear();
	Super::EndPlay(EndPlayReason);
}

void UHollowCosmeticPartsComponent::Clear()
{
	if (Loading.IsValid())
	{
		Loading->CancelHandle();
		Loading.Reset();
	}
	for (UStaticMeshComponent* Part : Parts)
	{
		if (Part)
		{
			Part->DestroyComponent();
		}
	}
	Parts.Reset();
}

void UHollowCosmeticPartsComponent::ApplySet(const UHollowCosmeticPartSet* Set)
{
	Clear();
	if (!Set)
	{
		return;
	}
	TArray<FSoftObjectPath> Paths;
	for (const FHollowCosmeticPart& Part : Set->Parts)
	{
		Paths.AddUnique(Part.Mesh.ToSoftObjectPath());
		for (const TSoftObjectPtr<UMaterialInterface>& Material : Part.Materials)
		{
			if (!Material.IsNull())
			{
				Paths.AddUnique(Material.ToSoftObjectPath());
			}
		}
	}
	if (!Paths.ContainsByPredicate([](const FSoftObjectPath& Path) { return Path.ResolveObject() == nullptr; }))
	{
		Attach(*Set);   // everything is in memory already: no frame without the parts
		return;
	}
	const TWeakObjectPtr<const UHollowCosmeticPartSet> WeakSet = Set;
	Loading = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths, FStreamableDelegate::CreateWeakLambda(this, [this, WeakSet]()
	{
		Loading.Reset();
		if (const UHollowCosmeticPartSet* Loaded = WeakSet.Get())
		{
			Attach(*Loaded);
		}
	}));
}

void UHollowCosmeticPartsComponent::Attach(const UHollowCosmeticPartSet& Set)
{
	AActor* Owner = GetOwner();
	USkeletalMeshComponent* BodyMesh = FindBody();
	const UWorld* World = GetWorld();
	if (!Owner || !BodyMesh || !IsRegistered() || !World || World->bIsTearingDown || !IsValid(Owner))
	{
		return;   // the load finished after its world went away
	}
	for (const FHollowCosmeticPart& Spec : Set.Parts)
	{
		UStaticMesh* Mesh = Spec.Mesh.Get();
		USkeletalMeshComponent* Parent = Spec.On == EHollowPartMesh::FirstPerson ? FirstPerson.Get() : BodyMesh;
		if (!Mesh || !Parent)
		{
			continue;   // no first-person hands on this pawn (an enemy, a test): the part has nowhere to go
		}
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(Owner, NAME_None, RF_Transient);
		Part->SetStaticMesh(Mesh);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Part->SetCanEverAffectNavigation(false);
		Part->SetGenerateOverlapEvents(false);
		Part->SetRelativeTransform(Spec.Offset);
		if (Spec.On == EHollowPartMesh::FirstPerson)
		{
			Part->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
			Part->SetOnlyOwnerSee(true);
			Part->SetCastShadow(false);
		}
		else
		{
			Part->SetFirstPersonPrimitiveType(Parent->FirstPersonPrimitiveType);   // the owner sees the body's shadow, not the body
		}
		for (int32 Slot = 0; Slot < Spec.Materials.Num(); ++Slot)
		{
			if (UMaterialInterface* Material = Spec.Materials[Slot].Get())
			{
				Part->SetMaterial(Slot, Material);
			}
		}
		Part->SetupAttachment(Parent, Spec.Bone);
		Part->RegisterComponent();
		if (bTinted)
		{
			Part->SetCustomPrimitiveDataVector4(TintDataIndex, FVector4(Tint));
		}
		Parts.Add(Part);
	}
}

void UHollowCosmeticPartsComponent::SetTint(const FLinearColor& InTint)
{
	Tint = InTint;
	bTinted = true;
	for (UStaticMeshComponent* Part : Parts)
	{
		if (Part)
		{
			Part->SetCustomPrimitiveDataVector4(TintDataIndex, FVector4(Tint));
		}
	}
}

#undef LOCTEXT_NAMESPACE
