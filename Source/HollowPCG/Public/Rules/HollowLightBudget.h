#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "HollowLightBudget.generated.h"

class UHollowRoomDefinition;
class ULevel;
class ULightComponent;

/** What one room type may light (ADR-0021, Block 3 sec. 3.9): shadows and light functions are the expensive part. */
USTRUCT(BlueprintType)
struct HOLLOWPCG_API FHollowRoomLightLimits
{
	GENERATED_BODY()

	/** Hollow.Room.Type.* these limits apply to. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light", meta = (Categories = "Hollow.Room.Type"))
	FGameplayTag RoomType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light", meta = (ClampMin = "0"))
	int32 MaxLights = 0;

	/** Lights casting (virtual) shadows. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light", meta = (ClampMin = "0"))
	int32 MaxShadowed = 0;

	/** Shadowed lights that also flicker (a light function). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light", meta = (ClampMin = "0"))
	int32 MaxShadowedFlickering = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light", meta = (ClampMin = "0"))
	int32 MaxLightFunctions = 0;

	/** Lights casting shadows into the volumetric fog. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light", meta = (ClampMin = "0"))
	int32 MaxVolumetricShadows = 0;

	/** A room one walks through between others (corridor, junction): a view from it can see two rooms besides. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light")
	bool bPassThrough = false;
};

/** One light of a room, as the budget sees it. */
struct HOLLOWPCG_API FHollowLightInfo
{
	FString Name;
	bool bShadowed = false;
	bool bLightFunction = false;
	bool bVolumetricShadow = false;
	/** 0 for lights without one (directional). */
	float AttenuationRadius = 0.f;
};

namespace HollowLightBudget
{
	/** The budget's view of a light component. */
	HOLLOWPCG_API FHollowLightInfo Describe(const ULightComponent& Light);

	/** Every light component of a level's actors. */
	HOLLOWPCG_API TArray<FHollowLightInfo> CollectLevel(const ULevel& Level);

	/** What breaks Limits (empty when within): counts per category, and radii beyond MaxRadius (0: no radius rule). */
	HOLLOWPCG_API TArray<FString> CheckRoom(TConstArrayView<FHollowLightInfo> Lights, const FHollowRoomLightLimits& Limits, float MaxRadius);

	/**
	 * Worst number of shadowed lights one view can hold: the two most shadowed room types (a room and the next one
	 * through a doorway, or the two ends seen from a pass-through room) plus the most shadowed pass-through type.
	 */
	HOLLOWPCG_API int32 WorstShadowedInView(TConstArrayView<FHollowRoomLightLimits> Rooms);

	/** No light of a room should reach farther than its own diagonal (footprint and storeys of UHollowLevelMetrics). */
	HOLLOWPCG_API float RoomReach(const UHollowRoomDefinition& Room);

	/** A room's level against the shipped budget (DA_LightBudget): what breaks it, or why it cannot be checked. */
	HOLLOWPCG_API TArray<FString> CheckRoomLevel(const UHollowRoomDefinition& Room, const ULevel& Level);
}

/** Light limits per room type and per frame (DA_LightBudget, UHollowLevelMetrics::LightBudget). */
UCLASS(BlueprintType)
class HOLLOWPCG_API UHollowLightBudgetConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** The budget named by UHollowLevelMetrics (loaded on first use), or null. */
	static const UHollowLightBudgetConfig* Get();

	/** Limits of a room with these tags (its Hollow.Room.Type.*), or null. */
	const FHollowRoomLightLimits* FindLimits(const FGameplayTagContainer& RoomTags) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light", meta = (TitleProperty = "RoomType"))
	TArray<FHollowRoomLightLimits> Rooms;

	/** Shadowed lights one frame may hold (Block 3 sec. 3.9: 4 on the RTX 3060). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light", meta = (ClampMin = "0"))
	int32 MaxShadowedInView = 0;
};
