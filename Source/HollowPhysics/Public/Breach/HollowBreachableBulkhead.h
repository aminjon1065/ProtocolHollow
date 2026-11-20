#pragma once

#include "Breach/HollowBreachTypes.h"
#include "GameFramework/Actor.h"
#include "Interfaces/HollowStructural.h"

#include "HollowBreachableBulkhead.generated.h"

class UBoxComponent;
class UGeometryCollectionComponent;
class UHollowStructuralIntegrityComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UNavArea;
class UNavLinkCustomComponent;
class UStaticMesh;
class UStaticMeshComponent;
struct FStreamableHandle;

/**
 * Breachable Chaos bulkhead (Docs/Architecture/03 sec. 3.4, ADR-0002, ADR-0014), spawned by the server in a Bulkhead
 * connection and dormant between changes. Gameplay geometry is authored and identical everywhere: a frame and one
 * panel per breach zone (UHollowBreachableDefinition); a zone's panel blocks until the replicated zone mask says it
 * broke. Each machine presents a break on its own: a live cosmetic fracture of the zone's geometry collection (Full)
 * or straight to static rubble (Snap), as UHollowDestructionBudgetSubsystem decides. Debris and rubble live on the
 * HollowDebris channel and never touch pawns or navigation. Navigation (server only, ADR-0004): NavBlocker keeps the
 * wall out of the navmesh until the breach, NavLink switches from the intact to the breached area at once, and the
 * blocker goes a little later (one bounded tile rebuild).
 */
UCLASS()
class HOLLOWPHYSICS_API AHollowBreachableBulkhead : public AActor, public IHollowStructural
{
	GENERATED_BODY()

public:
	explicit AHollowBreachableBulkhead(const FObjectInitializer& ObjectInitializer);

	UHollowStructuralIntegrityComponent* GetIntegrity() const { return Integrity; }
	UNavLinkCustomComponent* GetNavLink() const { return NavLink; }
	/** Area of the nav link (server: the navmesh exists only there). */
	TSubclassOf<UNavArea> GetNavArea() const { return NavArea; }
	bool IsNavBlockerActive() const;

	int32 GetZoneCount() const { return Panels.Num(); }
	UStaticMeshComponent* GetPanel(int32 Zone) const;
	UStaticMeshComponent* GetRubble(int32 Zone) const;
	/** The zone's live fracture while it plays, else null. */
	UGeometryCollectionComponent* GetLiveFracture(int32 Zone) const;
	/** How this machine showed the zone's break; unset while the zone stands. */
	TOptional<EHollowBreachPresentation> GetZonePresentation(int32 Zone) const;
	/** World centre of a zone's panel. */
	FVector GetZoneCenter(int32 Zone) const;

	/**
	 * Shows a broken zone. Full needs the zone's geometry collection loaded and a budget slot, otherwise it snaps.
	 * Called by the state handler with the policy's choice; tests call it directly.
	 */
	void PresentZoneBreak(int32 Zone, EHollowBreachPresentation Presentation, const FVector& ImpactDirection);

	/** Rigid bodies of a fracture: its leaf pieces (clustering is off in bulkhead collections; the box bone keeps no body). */
	static int32 CountFractureBodies(const class UGeometryCollection* Collection);

	// IHollowStructural
	virtual bool CanTakeStructuralDamage() const override;
	virtual bool ApplyStructuralDamage(const FHollowStructuralHit& Hit) override;
	/** Breach until the wall gives way (the nav link opens at the same moment), then Clear. */
	virtual EHollowPassage GetPassage() const override;

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Bulkhead")
	TObjectPtr<USceneComponent> Root;

	/** Lintel above the zones: always there, always blocks. */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Bulkhead")
	TObjectPtr<UStaticMeshComponent> Frame;

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Bulkhead")
	TObjectPtr<UHollowStructuralIntegrityComponent> Integrity;

	/** Null-area box across the wall (dynamic obstacle, no collision): the navmesh never crosses it until the breach. */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Bulkhead")
	TObjectPtr<UBoxComponent> NavBlocker;

	/** Two-way off-mesh link through the opening: intact area (Heavy may break through), then the breached area. */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Bulkhead")
	TObjectPtr<UNavLinkCustomComponent> NavLink;

	/** Link ends lie this far on either side of the wall, beyond NavBlocker, on the navmesh. */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Bulkhead", meta = (ClampMin = "0", Units = "Centimeters"))
	float NavLinkHalfLength = 0.f;

	/** Mesh of a zone panel, scaled to the zone box (its bounds give the scale). */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Bulkhead")
	TObjectPtr<UStaticMesh> PanelMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Bulkhead")
	TObjectPtr<UMaterialInterface> PanelMaterial;

	/** Static rubble under a broken zone, scaled to the zone width and the definition's rubble size. */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Bulkhead")
	TObjectPtr<UStaticMesh> RubbleMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Bulkhead")
	TObjectPtr<UMaterialInterface> RubbleMaterial;

	/** Panel material vector parameter tinted from IntactColor to DamagedColor with the damage stage. */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Bulkhead")
	FName DamageColorParameter;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Bulkhead")
	FLinearColor IntactColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Bulkhead")
	FLinearColor DamagedColor = FLinearColor::White;

private:
	void HandleStateChanged(const FHollowBreachNetState& Old, const FHollowBreachNetState& New);
	void PresentNewBreaks(const FHollowBreachNetState& State);
	EHollowBreachPresentation ChoosePresentation(int32 Zone, const FHollowBreachNetState& State) const;
	void UpdateDamageColor();
	void OpenPassage();
	void RemoveNavBlocker();
	void ShowRubble(int32 Zone);
	void HandleLiveExpired(int32 Zone);
	void EndLiveFracture(int32 Zone);
	void RequestFractureAssets();
	static FVector MeshHalfSize(const UStaticMesh* Mesh);

	TSubclassOf<UNavArea> NavArea;
	uint8 ShownMask = 0;
	TArray<TOptional<EHollowBreachPresentation>> Presented;
	TArray<int32> LiveHandles;
	FTimerHandle BlockerTimer;
	FDelegateHandle StateChangedHandle;
	TSharedPtr<FStreamableHandle> FractureHandle;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Panels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Rubble;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGeometryCollectionComponent>> LiveFractures;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> PanelMaterials;
};
