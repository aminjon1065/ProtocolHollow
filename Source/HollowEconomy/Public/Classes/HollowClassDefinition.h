#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Items/HollowItemTypes.h"

#include "HollowClassDefinition.generated.h"

class UGameplayEffect;
class UHollowAbilitySet;
struct FHollowClassTerms;

/**
 * A player class (ADR-0034), primary asset type HollowClass: Engineer, Techno-Occultist. Picked at the hub's locker
 * and kept in the player's profile (ADR-0005); while a player plays it the server grants its ability set, its
 * attribute effect and its tag (UHollowClassComponent). Its signature tools are items of its starting loadout.
 */
UCLASS(BlueprintType)
class HOLLOWECONOMY_API UHollowClassDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/** The plain terms the profile rules work with (HollowCraftingRules::SelectClass). */
	FHollowClassTerms ToTerms() const;

	/** Every class of the project, in PrimaryAssetId order (loaded synchronously: tiny data assets). */
	static TArray<FHollowClassTerms> LoadAll();

	/** The class Id names (loaded synchronously), or null for anything that is not a class. */
	static const UHollowClassDefinition* Load(const FPrimaryAssetId& Id);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Class")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Class", meta = (MultiLine = true))
	FText Description;

	/** On its players' ability systems while they play it (Hollow.Class.*): abilities and AI may ask. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Class", meta = (Categories = "Hollow.Class"))
	FGameplayTag ClassTag;

	/** The class's own abilities (its tools come as items): granted by the server while it is played. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Abilities")
	TSoftObjectPtr<UHollowAbilitySet> AbilitySet;

	/** An infinite effect with the class's attribute modifiers, applied while it is played. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Abilities")
	TSoftClassPtr<UGameplayEffect> AttributeEffect;

	/** The loadout a profile gets the first time it picks the class. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout", meta = (TitleProperty = "Item"))
	TArray<FHollowLoadoutSlot> StartingLoadout;

	/** Its players' visor telemetry colour. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visor")
	FLinearColor VisorColor = FLinearColor::White;
};
