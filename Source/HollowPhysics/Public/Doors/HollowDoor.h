#pragma once

#include "GameFramework/Actor.h"
#include "Interfaces/HollowInteractable.h"
#include "Interfaces/HollowToolTarget.h"

#include "HollowDoor.generated.h"

class UBoxComponent;
class UHollowDoorStateComponent;
class UMaterialInstanceDynamic;
class UNavArea;
class UNavLinkCustomComponent;
class UStaticMeshComponent;
struct FHollowDoorNetState;

/**
 * Sector door (Docs/Architecture/03 sec. 3.3), spawned by the server in a connection's opening and dormant between
 * changes. Welded and cut through IHollowToolTarget, opened and closed through IHollowInteractable; all rules live in
 * UHollowDoorStateComponent. The leaf moves from server time on every machine (timer during transitions, no Tick)
 * and never affects the navmesh. Navigation (S2-05, ADR-0004): NavCut keeps the opening out of the navmesh for good,
 * NavLink is the only way through, and a state change only swaps the link's area (no tile rebuild, server only).
 */
UCLASS()
class HOLLOWPHYSICS_API AHollowDoor : public AActor, public IHollowToolTarget, public IHollowInteractable
{
	GENERATED_BODY()

public:
	explicit AHollowDoor(const FObjectInitializer& ObjectInitializer);

	UHollowDoorStateComponent* GetDoorState() const { return DoorState; }
	UStaticMeshComponent* GetLeaf() const { return Leaf; }
	UNavLinkCustomComponent* GetNavLink() const { return NavLink; }
	/** Area of the nav link (server: the navmesh exists only there). */
	TSubclassOf<UNavArea> GetNavArea() const { return NavArea; }
	/** Leaf opening 0..1 as currently shown. */
	float GetLeafAlpha() const { return ShownAlpha; }
	bool IsDoorwayClear() const;

	// IHollowToolTarget
	virtual bool CanAcceptToolWork(const FHollowToolWork& Work) const override;
	virtual void ApplyToolWork(const FHollowToolWork& Work) override;

	// IHollowInteractable
	virtual FHollowInteractionInfo GetInteractionInfo(const AActor* Interactor) const override;
	virtual bool CanInteract(const AActor* Interactor) const override;
	virtual void Interact(AActor* Interactor) override;

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Door")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Door")
	TObjectPtr<UStaticMeshComponent> Leaf;

	/** The opening: pawns inside block closing; it also catches interaction traces when the leaf is up. */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Door")
	TObjectPtr<UBoxComponent> Doorway;

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Door")
	TObjectPtr<UHollowDoorStateComponent> DoorState;

	/** Null-area cut across the opening (dynamic obstacle, no collision): the navmesh never crosses it by itself. */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Door")
	TObjectPtr<UBoxComponent> NavCut;

	/** Two-way off-mesh link through the opening; its area follows the door state. */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Door")
	TObjectPtr<UNavLinkCustomComponent> NavLink;

	/** Link ends lie this far on either side of the opening, beyond NavCut, on the navmesh. */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Door", meta = (ClampMin = "0", Units = "Centimeters"))
	float NavLinkHalfLength = 0.f;

	/** Where the open leaf sits relative to its closed position. */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Door")
	FVector LeafOpenOffset = FVector::ZeroVector;

	/** Leaf refresh rate while it moves. */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Door", meta = (ClampMin = "1", Units = "Hertz"))
	float LeafUpdateRateHz = 0.f;

	/** Leaf material vector parameter tinted from ClosedColor to WeldedColor with the weld. */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Door")
	FName WeldColorParameter;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Door")
	FLinearColor ClosedColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Door")
	FLinearColor WeldedColor = FLinearColor::White;

private:
	void HandleStateChanged(const FHollowDoorNetState& Old, const FHollowDoorNetState& New, bool bFresh);
	void UpdateLeaf();
	void UpdateWeldColor();
	void UpdateNavArea();

	TSubclassOf<UNavArea> NavArea;

	FVector LeafClosedLocation = FVector::ZeroVector;
	float ShownAlpha = 0.f;
	FTimerHandle LeafTimer;
	FDelegateHandle StateChangedHandle;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LeafMaterial;
};
