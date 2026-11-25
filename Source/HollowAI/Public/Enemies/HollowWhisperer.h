#pragma once

#include "Enemies/HollowEnemyCharacter.h"

#include "HollowWhisperer.generated.h"

class UAudioComponent;
class UStaticMeshComponent;

/**
 * A Whisperer (GP-02, ADR-0047): a small, fast enemy of a swarm, driven by AHollowWhispererController. Its body is a
 * static mesh (no skeleton; the legs' scuttle is the material's, on the GPU) without collision: the capsule is what
 * shots, blows and lights find. Presentation follows its replicated tags on every machine: a beam on it
 * (Hollow.State.Lit) makes it shriek and its eyes flare, a frenzy (Hollow.State.Frenzy) makes its whisper louder; it
 * whispers all the while it lives, and dies with a crunch, its eyes out and its legs still.
 */
UCLASS()
class HOLLOWAI_API AHollowWhisperer : public AHollowEnemyCharacter
{
	GENERATED_BODY()

public:
	explicit AHollowWhisperer(const FObjectInitializer& ObjectInitializer);

	UStaticMeshComponent* GetBody() const { return Body; }
	/** Its whisper on this machine (none on a dedicated server, without an audio device, or once dead). */
	UAudioComponent* GetWhisper() const { return Whisper; }
	/** Shrieks this machine has played for it (tests, the smoke logs). */
	int32 GetShriekCount() const { return Shrieks; }

	/** Custom primitive data of its body that its material reads: the eyes' flare (0..1) and death (0 or 1). */
	static constexpr int32 FlareDataIndex = 0;
	static constexpr int32 DeadDataIndex = 1;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PresentDeath() override;

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Enemy")
	TObjectPtr<UStaticMeshComponent> Body;

private:
	void HandleLitChanged(const FGameplayTag Tag, int32 Count);
	void HandleFrenzyChanged(const FGameplayTag Tag, int32 Count);
	void StopWhisper();

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Whisper;

	/** The whisper's volume as the bank plays it (a frenzy scales it). */
	float WhisperVolume = 1.f;
	int32 Shrieks = 0;
	FDelegateHandle LitHandle;
	FDelegateHandle FrenzyHandle;
};
