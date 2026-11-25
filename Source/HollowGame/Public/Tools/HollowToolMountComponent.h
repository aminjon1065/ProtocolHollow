#pragma once

#include "Components/ActorComponent.h"

#include "HollowToolMountComponent.generated.h"

class UCameraComponent;
class UHollowToolDefinition;
class UHollowWeaponDefinition;
class UStaticMesh;
class UStaticMeshComponent;
struct FStreamableHandle;

/**
 * Where a pawn holds its tool (ADR-0027, art pass): two mesh components it drives, one in the first-person hands
 * (first-person rendering, only the owner sees it) and one on the body (everyone else; the owner sees its shadow),
 * each attached to a hand socket (data). It shows the held mesh of the tool in use (UHollowToolDefinition::HeldMesh,
 * loaded asynchronously) and tells where the beam leaves it: the muzzle socket as this machine sees it. Purely local
 * cosmetics: which tool is shown follows the replicated beam cue, nothing more is sent. Never ticks.
 */
UCLASS(ClassGroup = Hollow, meta = (BlueprintSpawnableComponent))
class HOLLOWGAME_API UHollowToolMountComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHollowToolMountComponent();

	static UHollowToolMountComponent* Get(const AActor* Owner);

	/** The meshes it drives and the camera the first-person one is seen through (the owner's constructor). */
	void Setup(UStaticMeshComponent* InFirstPersonMesh, UStaticMeshComponent* InThirdPersonMesh, UCameraComponent* InCamera);

	/** Attaches the meshes to the hand sockets of the first-person and the body meshes. */
	void AttachToHands(USceneComponent* FirstPersonParent, USceneComponent* ThirdPersonParent);

	/** Shows Tool's held mesh in both hands (none: empty hands): the tool in use. */
	void ShowTool(const UHollowToolDefinition* Tool);

	/**
	 * GP-01: the weapon in hand instead of the tool (its held mesh, transform, muzzle and first-person view offset); none:
	 * back to the tool shown before. UHollowHandsComponent calls it on every machine.
	 */
	void SetHeldWeapon(const UHollowWeaponDefinition* Weapon);
	const UHollowWeaponDefinition* GetHeldWeapon() const { return HeldWeapon.Get(); }

	/** Art-01: the class's own tool replaces DefaultTool as long as no other tool was used (the Occultist holds the lantern). */
	void SetClassTool(const UHollowToolDefinition* Tool);

	/** Art-01: the held tool glows while it runs (custom primitive data FiringDataIndex of both meshes, 0 or 1). */
	void SetFiring(bool bFiring);
	bool IsFiring() const { return bFiringShown; }

	/** Index of the firing flag in the held meshes' custom primitive data (the tools' glow material reads it). */
	static constexpr int32 FiringDataIndex = 0;
	const UHollowToolDefinition* GetShownTool() const { return ShownTool.Get(); }

	/**
	 * Where the beam leaves the held tool as this machine sees it: the owner's first-person muzzle moved into world
	 * space the way first-person rendering draws it (full size), everyone else's the body's muzzle. False without a held
	 * mesh that has the tool's muzzle socket.
	 */
	bool GetMuzzleLocation(FVector& OutLocation) const;

	/**
	 * GP-01c (ADR-0046): the first-person tool off its rest pose by Offset in the camera's space (the view feel's sway,
	 * recoil and dip, UHollowViewFeelModifier). Only while it rides the camera; the body's tool never moves.
	 */
	void SetViewFeelOffset(const FTransform& Offset);
	const FTransform& GetViewFeelOffset() const { return ViewFeel; }

	/** Whether this machine sees the owner in first person (its own local player). */
	bool IsFirstPersonView() const;

	UStaticMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh.Get(); }
	UStaticMeshComponent* GetThirdPersonMesh() const { return ThirdPersonMesh.Get(); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Socket of the first-person hands the tool sits in. */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Tool")
	FName FirstPersonSocket;

	/** Socket of the body mesh the tool sits in. */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Tool")
	FName ThirdPersonSocket;

	/**
	 * Art-01 (ADR-0043): the first-person tool rides the camera at FirstPersonViewOffset instead of the hand (the
	 * placeholder arms have no first-person holding pose, so a tool in the hand stays out of view). Everyone else
	 * still sees it in the body's hand; the beam leaves the muzzle the owner sees either way.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Tool")
	bool bFirstPersonOnCamera = false;

	/** Where the first-person tool sits in the camera's space (forward, right, up) while it rides the camera. */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Tool", meta = (EditCondition = "bFirstPersonOnCamera"))
	FTransform FirstPersonViewOffset;

	/** In hand from the start, until another tool is used. */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Tool")
	TSoftObjectPtr<UHollowToolDefinition> DefaultTool;

private:
	void ApplyMesh();
	/** Loads the held mesh of what is in hand (the weapon, else the shown tool), then applies it. */
	void LoadAndApply();
	/** The held item's mesh, transform relative to the hand, muzzle socket; the first-person view offset if it has its own. */
	void GetHeld(TSoftObjectPtr<UStaticMesh>& OutMesh, FTransform& OutTransform, FName& OutMuzzle, const FTransform*& OutViewOffset) const;
	/** The first-person tool's pose on the camera: Rest moved by the view feel. */
	FTransform WithViewFeel(const FTransform& Rest) const;
	bool IsFirstPersonOnCamera() const;

	/** Shows the class's tool, else DefaultTool, until a tool is used. */
	void ShowDefault();
	void Show(const UHollowToolDefinition* Tool);

	TWeakObjectPtr<UStaticMeshComponent> FirstPersonMesh;
	TWeakObjectPtr<UStaticMeshComponent> ThirdPersonMesh;
	TWeakObjectPtr<UCameraComponent> Camera;
	TWeakObjectPtr<const UHollowToolDefinition> ShownTool;
	TWeakObjectPtr<const UHollowWeaponDefinition> HeldWeapon;
	TSharedPtr<FStreamableHandle> Loading;
	TWeakObjectPtr<const UHollowToolDefinition> ClassTool;
	/** No tool used yet: the hands show the default (the class's tool once it is known). */
	bool bShowingDefault = true;
	bool bFiringShown = false;
	/** The view feel's offset, and the rest pose on the camera it moves the tool from. */
	FTransform ViewFeel = FTransform::Identity;
	FTransform ViewRest = FTransform::Identity;
};
