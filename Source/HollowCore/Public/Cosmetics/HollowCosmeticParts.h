#pragma once

#include "Components/ActorComponent.h"
#include "Engine/DataAsset.h"

#include "HollowCosmeticParts.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
struct FStreamableHandle;

/** Which of a character's meshes a part rides on (Art-01, ADR-0043). */
UENUM(BlueprintType)
enum class EHollowPartMesh : uint8
{
	/** The body everyone sees (its first-person setting is copied: the owner of a first-person pawn sees its shadow). */
	Body,
	/** The first-person hands only the owner sees (bracers, gloves). */
	FirstPerson
};

/** One piece of armour or anatomy on a bone: a static mesh, no collision, purely cosmetic. */
USTRUCT(BlueprintType)
struct FHollowCosmeticPart
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part")
	EHollowPartMesh On = EHollowPartMesh::Body;

	/** The bone (or socket) it follows. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part")
	FName Bone;

	/** Where it sits in the bone's space. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part")
	FTransform Offset;

	/** One per material slot of Mesh (a null one keeps the mesh's own). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part")
	TArray<TSoftObjectPtr<UMaterialInterface>> Materials;
};

/**
 * Cosmetic parts of a character (Art-01): what armoured a placeholder skeleton. CHAR-01 (ADR-0055): the characters wear
 * their own meshes, so none ships a set; a set stays possible for an armoured variant.
 */
UCLASS(BlueprintType)
class HOLLOWCORE_API UHollowCosmeticPartSet : public UDataAsset
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts")
	TArray<FHollowCosmeticPart> Parts;
};

/** One colour parameter of a character's own materials the tint drives (CHAR-01: the ExoFrame's visor glow). */
USTRUCT(BlueprintType)
struct FHollowTintParameter
{
	GENERATED_BODY()

	/** A vector parameter of the body's (and the first-person hands') materials. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tint")
	FName Parameter;

	/** The tint times this (an emissive's brightness: the material's own colour is HDR). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tint", meta = (ClampMin = "0"))
	float Intensity = 1.f;
};

/**
 * Puts a part set on a character's skeletal meshes (Art-01, ADR-0043): loads it asynchronously at BeginPlay and
 * attaches one static mesh component per part to its bone, without collision or navigation, seen as its mesh is
 * seen (first-person rendering). Every machine does it locally; nothing replicates. A tint goes to the parts' custom
 * primitive data (index 0..3, RGBA): the materials that read it glow in the class's colour. CHAR-01 (ADR-0055): it
 * also goes to TintParameters of the body's and the hands' own materials (a dynamic instance per slot that has one).
 * Never ticks.
 */
UCLASS(ClassGroup = Hollow, meta = (BlueprintSpawnableComponent))
class HOLLOWCORE_API UHollowCosmeticPartsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHollowCosmeticPartsComponent();

	static UHollowCosmeticPartsComponent* Get(const AActor* Owner);

	/** The meshes the parts ride on (the owner's constructor); without them the owner's first skeletal mesh is the body. */
	void Setup(USkeletalMeshComponent* InBody, USkeletalMeshComponent* InFirstPerson);

	/** Puts the set on now (BeginPlay does with PartSet); null takes the parts off. */
	void ApplySet(const UHollowCosmeticPartSet* Set);

	/** The tint of every part (custom primitive data 0..3), now and for parts still loading. */
	void SetTint(const FLinearColor& InTint);
	const FLinearColor& GetTint() const { return Tint; }

	/** The body's and hands' material instances the tint drives (tests, Hollow.Art.Dump). */
	const TArray<TObjectPtr<UMaterialInstanceDynamic>>& GetTintedMaterials() const { return TintedMaterials; }

	/** The parts attached so far (tests, Hollow.Art.Dump). */
	const TArray<TObjectPtr<UStaticMeshComponent>>& GetPartComponents() const { return Parts; }
	bool IsLoading() const;

	/** Index of the tint in custom primitive data (the materials' "CustomPrimitiveData" start index). */
	static constexpr int32 TintDataIndex = 0;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Art")
	TSoftObjectPtr<UHollowCosmeticPartSet> PartSet;

	/** CHAR-01: the colour parameters of the body's and the hands' own materials the tint drives too. */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Art")
	TArray<FHollowTintParameter> TintParameters;

private:
	void Clear();
	void Attach(const UHollowCosmeticPartSet& Set);
	USkeletalMeshComponent* FindBody() const;
	/** The tint on TintParameters of the body's and the hands' materials (a dynamic instance per slot, made once). */
	void TintMeshes();
	bool HasTintParameter(const UMaterialInterface& Material) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> TintedMaterials;

	TWeakObjectPtr<USkeletalMeshComponent> Body;
	TWeakObjectPtr<USkeletalMeshComponent> FirstPerson;
	TSharedPtr<FStreamableHandle> Loading;
	FLinearColor Tint = FLinearColor::White;
	bool bTinted = false;
};
