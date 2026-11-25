#pragma once

#include "Components/ActorComponent.h"

#include "HollowViewFeelComponent.generated.h"

class UAbilitySystemComponent;
class UHollowViewFeelModifier;
struct FOnAttributeChangeData;

/**
 * An ExoFrame's game feel on its own player's machine (GP-01c, ADR-0046): once a local player controls it, the view
 * feel on that player's camera (UHollowFeelSubsystem::GetViewModifier) moves the first-person tool (its sway, recoil
 * and dip through UHollowToolMountComponent), and the health it loses jolts the view (Hollow.Feel.Damage, stronger the
 * more it lost). Elsewhere it does nothing. Never ticks: the camera drives the pose, the health its change delegate.
 */
UCLASS(ClassGroup = Hollow, meta = (BlueprintSpawnableComponent))
class HOLLOWGAME_API UHollowViewFeelComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	explicit UHollowViewFeelComponent(const FObjectInitializer& ObjectInitializer);

	static UHollowViewFeelComponent* Get(const AActor* Owner);

	/** The pawn's controller or ability system changed (the pawn calls it): binds to its local player's view and health, or lets go. */
	void Refresh();

	/** Whether it follows a local player's view now. */
	bool IsBound() const { return BoundView.IsValid(); }

	/** Health lost to a damage feel of this strength (0..1): Lost / MaxHealth / FullJoltShare. */
	float DamageFeelScale(float Lost, float MaxHealth) const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Losing this share of its health at once is the strongest jolt; less is weaker in proportion. */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Feel", meta = (ClampMin = "0.01", ClampMax = "1"))
	float FullJoltShare = 0.25f;

private:
	void Unbind();
	void HandleHealthChanged(const FOnAttributeChangeData& Change);

	TWeakObjectPtr<UHollowViewFeelModifier> BoundView;
	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystem;
	FDelegateHandle PoseHandle;
	FDelegateHandle HealthHandle;
};
