#pragma once

#include "Camera/CameraModifier.h"
#include "Feel/HollowFeelTypes.h"

#include "HollowViewFeelModifier.generated.h"

class UHollowFeelConfig;

/** Where the first-person hands sit against their rest pose this frame (relative to the camera). */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnHollowHandsPose, const FTransform& /*Pose*/);

/**
 * One local player's view feel (GP-01c, ADR-0046), on its camera manager: the view kicks of its own shots, blows and
 * hand swaps (FHollowViewKick, springing back) and the sway of its first-person hands behind the aim. It turns only the
 * camera, never the control rotation, so the aim and the server's checks stay true. The hands' pose goes out through
 * OnHandsPose (the pawn moves its first-person mesh). The camera runs it each frame it renders: the one per-frame piece
 * of game feel, and it lives in the camera, not in an actor's tick.
 */
UCLASS()
class HOLLOWCORE_API UHollowViewFeelModifier : public UCameraModifier
{
	GENERATED_BODY()

public:
	static constexpr int32 MaxKicks = 8;

	/** The sway's tuning (UHollowFeelConfig); none: no sway. */
	void SetConfig(const UHollowFeelConfig* InConfig);

	/** Kick now at Scale (the oldest gives way past MaxKicks). */
	void AddKick(const FHollowViewKick& Kick, float Scale);

	/** One frame: the hands sway by how the aim turned since the last, the kicks age by DeltaTime. Returns the view's turn. */
	FRotator Step(float DeltaTime, const FRotator& ControlRotation);

	FRotator GetViewOffset() const { return ViewOffset; }
	const FTransform& GetHandsPose() const { return HandsPose; }
	FRotator GetSway() const { return Sway; }
	int32 GetKickCount() const { return Kicks.Num(); }

	FOnHollowHandsPose OnHandsPose;

protected:
	virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;

private:
	struct FKick
	{
		FHollowViewKick Params;
		float Scale = 0.f;
		float Elapsed = 0.f;
		float YawSign = 1.f;
	};

	UPROPERTY(Transient)
	TObjectPtr<const UHollowFeelConfig> Config;

	TArray<FKick, TInlineAllocator<MaxKicks>> Kicks;
	FRotator LastControl = FRotator::ZeroRotator;
	FRotator Sway = FRotator::ZeroRotator;
	FRotator ViewOffset = FRotator::ZeroRotator;
	FTransform HandsPose = FTransform::Identity;
	uint32 KickSerial = 0;
	bool bHasLastControl = false;
	/** The hands were last told a pose off their rest (so the return to rest is told once too). */
	bool bHandsOffRest = false;
};
