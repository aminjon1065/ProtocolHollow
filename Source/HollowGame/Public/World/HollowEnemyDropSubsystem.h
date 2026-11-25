#pragma once

#include "Subsystems/WorldSubsystem.h"

#include "HollowEnemyDropSubsystem.generated.h"

class AHollowEnemyCharacter;
class AHollowLootPickup;

/**
 * What dead enemies leave (GP-02, ADR-0047), server only: an enemy whose definition has a drop (FHollowEnemyDrop) and
 * whose spawn seed rolls it (a Whisperer's: its nest's seed and its place in the brood) leaves a pickup of the item
 * where it died, popped up out of the body: found loot like a crate's (GP-01b). The AI says it died
 * (AHollowEnemyCharacter::OnAnyEnemyDied); this spawns the pickup.
 */
UCLASS()
class HOLLOWGAME_API UHollowEnemyDropSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UHollowEnemyDropSubsystem* Get(const UObject* WorldContext);

	/** Pickups dropped in this world so far, the last one (tests, the smoke logs). */
	int32 GetDropCount() const { return Drops; }
	AHollowLootPickup* GetLastDrop() const { return LastDrop.Get(); }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	void HandleEnemyDied(AHollowEnemyCharacter& Enemy);

	FDelegateHandle DiedHandle;
	int32 Drops = 0;
	TWeakObjectPtr<AHollowLootPickup> LastDrop;
};
