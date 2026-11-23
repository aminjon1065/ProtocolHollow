#pragma once

#include "Components/ActorComponent.h"
#include "Engine/DataAsset.h"

#include "HollowCosmeticParts.generated.h"

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

/** A character's cosmetic parts (DA_Parts_*): what turns a placeholder skeleton into an ExoFrame, a Pilgrim, a Stalker. */
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

/**
 * Puts a part set on a character's skeletal meshes (Art-01, ADR-0043): loads it asynchronously at BeginPlay and
 * attaches one static mesh component per part to its bone, without collision or navigation, seen as its mesh is
 * seen (first-person rendering). Every machine does it locally; nothing replicates. A tint goes to the parts' custom
 * primitive data (index 0..3, RGBA): the materials that read it glow in the class's colour. Never ticks.
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

private:
	void Clear();
	void Attach(const UHollowCosmeticPartSet& Set);
	USkeletalMeshComponent* FindBody() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	TWeakObjectPtr<USkeletalMeshComponent> Body;
	TWeakObjectPtr<USkeletalMeshComponent> FirstPerson;
	TSharedPtr<FStreamableHandle> Loading;
	FLinearColor Tint = FLinearColor::White;
	bool bTinted = false;
};
