#pragma once

#include "Components/SceneComponent.h"
#include "FX/HollowFx.h"

#include "HollowFxPlayerComponent.generated.h"

class UFXSystemComponent;
struct FStreamableHandle;

/**
 * UI-02 (ADR-0052): plays an effect of the data where it is placed, for as long as its actor plays, on this machine
 * only (a dressing, not gameplay: the main menu's Aether core). It loads the effect with itself and plays the
 * player's choice (Reduced Flashes: the dimmer variant, or nothing), switching as soon as that choice changes.
 * No tick: the effect simulates itself.
 */
UCLASS(ClassGroup = Hollow, meta = (BlueprintSpawnableComponent))
class HOLLOWGAME_API UHollowFxPlayerComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	explicit UHollowFxPlayerComponent(const FObjectInitializer& ObjectInitializer);

	/** The effect playing now (null before it loads, on a dedicated server, or when nothing plays for this player). */
	UFXSystemComponent* GetPlaying() const { return Playing; }

	const FHollowFxSpec& GetEffect() const { return Effect; }

	/** Another effect (tests; a level sets its own): played at once if the actor plays already. */
	void SetEffect(const FHollowFxSpec& InEffect);

	/** Plays the player's choice of the effect now (again: what played stops). */
	void Restart();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX")
	FHollowFxSpec Effect;

private:
	UPROPERTY(Transient)
	TObjectPtr<UFXSystemComponent> Playing;

	TSharedPtr<FStreamableHandle> FxHandle;
	FDelegateHandle FlashesHandle;
};
