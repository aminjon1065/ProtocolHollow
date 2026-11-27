#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "HollowAnimationAuthoring.generated.h"

class UAnimMontage;
class UAnimSequence;
class UAnimSequenceBase;
class UBlendSpace;
class USkeletalMesh;
class USkeleton;

/** One axis of a blend space (CHAR-01). */
USTRUCT(BlueprintType)
struct FHollowBlendAxis
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend")
	float Min = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend")
	float Max = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend")
	int32 GridDivisions = 4;

	/** The axis wraps round (a direction from -180 to 180). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend")
	bool bWrap = false;
};

/** One sample of a blend space: an animation at a point, played this much faster (CHAR-01). */
USTRUCT(BlueprintType)
struct FHollowBlendSampleSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend")
	TObjectPtr<UAnimSequence> Animation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend")
	FVector Value = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend")
	float RateScale = 1.f;
};

/** One stretch of an animation a montage plays, after the one before it (CHAR-01). */
USTRUCT(BlueprintType)
struct FHollowMontageSegment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage")
	TObjectPtr<UAnimSequenceBase> Animation;

	/** Seconds into the animation it starts at. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage")
	float StartTime = 0.f;

	/** Seconds into the animation it ends at (<= 0: the animation's end). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage")
	float EndTime = 0.f;
};

/** One section of a montage; Next == Name loops it (CHAR-01). */
USTRUCT(BlueprintType)
struct FHollowMontageSection
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage")
	FName Name;

	/** Seconds from the montage's start. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage")
	float Time = 0.f;

	/** The section played after it (none: the montage ends). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage")
	FName Next;
};

/**
 * Animation authoring for the content scripts (CHAR-01, ADR-0055; editor only, ADR-0010): what Python cannot reach of
 * blend spaces (axes, samples, rate scales), montages (slot, trimmed segment, sections) and skeletons (sockets, slots).
 * create_character_anim_assets.py calls it; each function makes the asset exactly as asked, so a second run changes
 * nothing.
 */
UCLASS()
class HOLLOWEDITOR_API UHollowAnimationAuthoring : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Makes BlendSpace's axes Axes (one for a 1D blend space) and its samples exactly Samples, then resamples it.
	 * False if an axis or a sample is invalid (no animation, of another skeleton).
	 */
	UFUNCTION(BlueprintCallable, Category = "Hollow|Editor|Animation")
	static bool SetBlendSpace(UBlendSpace* BlendSpace, const TArray<FHollowBlendAxis>& Axes, const TArray<FHollowBlendSampleSpec>& Samples);

	/** "axis <name> <min>..<max> /<grid> [wrap]" and "sample <animation> at (x, y) x<rate>", for the scripts' checks. */
	UFUNCTION(BlueprintCallable, Category = "Hollow|Editor|Animation")
	static TArray<FString> DescribeBlendSpace(const UBlendSpace* BlendSpace);

	/**
	 * Makes Montage one slot track (SlotName) playing Segments one after another, with exactly Sections and the blend
	 * times given. False if something is missing or a section falls outside.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hollow|Editor|Animation")
	static bool SetMontage(UAnimMontage* Montage, FName SlotName, const TArray<FHollowMontageSegment>& Segments,
		const TArray<FHollowMontageSection>& Sections, float BlendInSeconds, float BlendOutSeconds);

	/** "slot <name>", "segment <animation> <start>..<end>", "section <name> at <time> -> <next>", for the scripts' checks. */
	UFUNCTION(BlueprintCallable, Category = "Hollow|Editor|Animation")
	static TArray<FString> DescribeMontage(const UAnimMontage* Montage);

	/**
	 * Turns off every section (all LODs) of Mesh that wears one of the material Slots: not drawn, no depth, no shadow
	 * (the mesh editor's "Disable"), then rebuilds it. Returns how many sections it turned off (0: already). CHAR-01:
	 * what a Fab character held, hidden (fab_manifest.json hidden_slots) without its cost.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hollow|Editor|Animation")
	static int32 DisableMeshSlots(USkeletalMesh* Mesh, const TArray<FName>& Slots);

	/** How many of Mesh's sections (all LODs) wearing one of Slots are turned off, of how many. */
	UFUNCTION(BlueprintCallable, Category = "Hollow|Editor|Animation")
	static FIntPoint CountDisabledSections(const USkeletalMesh* Mesh, const TArray<FName>& Slots);

	/** Makes Skeleton's socket SocketName sit on Bone at Relative (added or moved). False without the bone. */
	UFUNCTION(BlueprintCallable, Category = "Hollow|Editor|Animation")
	static bool SetSkeletonSocket(USkeleton* Skeleton, FName SocketName, FName Bone, const FTransform& Relative);

	/** Registers SlotName in Skeleton's default slot group if it is not there (a montage's slot must be known). */
	UFUNCTION(BlueprintCallable, Category = "Hollow|Editor|Animation")
	static bool EnsureSkeletonSlot(USkeleton* Skeleton, FName SlotName);
};
