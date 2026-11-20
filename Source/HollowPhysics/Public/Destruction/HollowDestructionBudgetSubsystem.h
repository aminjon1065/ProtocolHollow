#pragma once

#include "Destruction/HollowDestructionBudget.h"
#include "Subsystems/WorldSubsystem.h"

#include "HollowDestructionBudgetSubsystem.generated.h"

/**
 * Cosmetic destruction budget of this machine (Docs/Architecture/03 sec. 3.4, Block 2 subsystem table): every machine
 * (host included) counts its own live fractures and rigid bodies against the scalability preset and ends each live
 * fracture after the budget's debris lifetime (timers, no Tick). Nothing here is replicated or affects gameplay.
 */
UCLASS()
class HOLLOWPHYSICS_API UHollowDestructionBudgetSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Budget at the current sg.EffectsQuality. */
	FHollowDestructionBudget GetBudget() const;

	/** Location of the first local player's camera; unset without a local player (server-only and test worlds). */
	TOptional<FVector> GetViewLocation() const;

	/** Presentation of a break here and now: fills the budget counts and asks HollowBreachPolicy. */
	EHollowBreachPresentation Choose(FHollowBreachPresentationInput Input) const;

	/**
	 * Registers a live fracture of Bodies rigid bodies. OnExpired runs once the budget's debris lifetime has passed
	 * (the owner then removes the debris and calls EndLive). Returns a handle, or INDEX_NONE if over budget.
	 */
	int32 BeginLive(int32 Bodies, FSimpleDelegate OnExpired);

	/** Ends a live fracture early or after OnExpired; unknown handles are ignored. */
	void EndLive(int32 Handle);

	int32 GetLiveBreaches() const { return Live.Num(); }
	int32 GetLiveBodies() const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;

private:
	struct FLiveBreach
	{
		int32 Handle = INDEX_NONE;
		int32 Bodies = 0;
		FTimerHandle Timer;
		FSimpleDelegate OnExpired;
	};

	void Expire(int32 Handle);

	TArray<FLiveBreach> Live;
	int32 NextHandle = 0;
};
