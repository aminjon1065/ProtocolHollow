#pragma once

#include "Components/ActorComponent.h"

#include "HollowToolMountComponent.generated.h"

class UCameraComponent;
class UHollowToolDefinition;
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

	/** Shows Tool's held mesh in both hands (none: empty hands). */
	void ShowTool(const UHollowToolDefinition* Tool);
	const UHollowToolDefinition* GetShownTool() const { return ShownTool.Get(); }

	/**
	 * Where the beam leaves the held tool as this machine sees it: the owner's first-person muzzle moved into world
	 * space the way first-person rendering draws it (full size), everyone else's the body's muzzle. False without a held
	 * mesh that has the tool's muzzle socket.
	 */
	bool GetMuzzleLocation(FVector& OutLocation) const;

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

	/** In hand from the start, until another tool is used. */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Tool")
	TSoftObjectPtr<UHollowToolDefinition> DefaultTool;

private:
	void ApplyMesh();

	TWeakObjectPtr<UStaticMeshComponent> FirstPersonMesh;
	TWeakObjectPtr<UStaticMeshComponent> ThirdPersonMesh;
	TWeakObjectPtr<UCameraComponent> Camera;
	TWeakObjectPtr<const UHollowToolDefinition> ShownTool;
	TSharedPtr<FStreamableHandle> Loading;
};
