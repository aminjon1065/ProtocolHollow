#pragma once

#include "Brain/HollowAIController.h"
#include "Interfaces/HollowLightSource.h"

#include "HollowWhispererController.generated.h"

class UHollowWhispererDefinition;
struct FEnvQueryResult;

/**
 * Brain body of a Whisperer (GP-02, ADR-0047): the enemy controller plus two activities of its own, in a swarm
 * (UHollowSwarmSubsystem). Recoil: a beam that catches it (Hollow.State.Lit, or an Aether lantern's Exposed) makes it
 * freeze with a shriek, a bite under way undone, then back away from that light; not again before RecoilCooldownSeconds.
 * Flank: while its prey has a light (or the others bite it), it goes to a spot by its prey no light reaches (FlankQuery)
 * and lunges from there. It hunts the player in the dark first; in a frenzy (Hollow.State.Frenzy) it bites whenever it
 * can. It opens plain doors and breaks nothing.
 */
UCLASS()
class HOLLOWAI_API AHollowWhispererController : public AHollowAIController
{
	GENERATED_BODY()

public:
	explicit AHollowWhispererController(const FObjectInitializer& ObjectInitializer);

	const UHollowWhispererDefinition* GetWhispererDefinition() const;

	/** Hollow.State.Lit on its pawn: a beam is on it (GE_Whisperer_Lit). */
	bool IsLit() const;
	/** Hollow.State.Frenzy on its pawn: dark, and its prey without light. */
	bool IsFrenzied() const;

	/** A light reached it (the swarm's light check): where it backs away from if it flinches. */
	void NoteLight(const FHollowLightCone& Cone);
	/** Where the last light that reached it came from (its origin), and whether there was one. */
	bool GetLastLight(FVector& OutOrigin) const;

	/** A beam caught it, it is not on its cooldown, and it has not flinched from it yet. */
	bool ShouldRecoil() const { return bRecoilPending; }
	/** Its bite may start now (the swarm's bite slots). */
	bool CanBite() const;
	/** It has prey with a light (and is not in a frenzy, nor lunging), or it is not its turn to bite. */
	bool ShouldFlank() const;
	/** Whether its prey casts any light now. */
	bool PreyHasLight() const;

	/** Times it flinched (tests, the smoke logs). */
	int32 GetRecoilCount() const { return Recoils; }

	virtual FString Describe() const override;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void StartActivity(EHollowAIActivity NewActivity) override;
	virtual void HandleActivityMove(bool bSuccess) override;
	virtual void OnActivityEnded(EHollowAIActivity Ended) override;
	virtual void OnExposed() override;
	/** A flinch plays out: a sight or a sound does not cut it short (the light's next check, or its end, decides). */
	virtual bool ShouldReconsiderOnPerception() const override { return Activity != EHollowAIActivity::Recoil; }
	virtual double TargetCost(const APawn& Candidate) const override;
	virtual void OnNewTarget(APawn& NewTarget) override;

private:
	void HandleLitChanged(const FGameplayTag Tag, int32 Count);
	/** A light caught it: it flinches unless on its cooldown. */
	void ArmRecoil();
	void StartRecoil();
	void BackAway();
	void StartFlank();
	void HandleFlankQuery(TSharedPtr<FEnvQueryResult> Result);
	/** No dark way round: it rushes in for a lunge. */
	void Lunge();

	FDelegateHandle LitHandle;
	FVector LastLightOrigin = FVector::ZeroVector;
	bool bHasLight = false;
	bool bRecoilPending = false;
	double RecoilReadyAt = 0.0;
	/** Until then it goes straight for its prey instead of flanking (it came out of the dark). */
	double LungeUntil = 0.0;
	int32 FlankQueryId = INDEX_NONE;
	int32 Recoils = 0;
};
