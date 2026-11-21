#pragma once

#include "Net/VoiceConfig.h"

#include "HollowVoiceTalker.generated.h"

class APawn;
class APlayerState;
class UHollowVoiceTalker;

/** A player started or stopped being heard on this machine (the visor shows who talks, S4-08). */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnHollowTalkingChanged, const UHollowVoiceTalker* /*Talker*/, bool /*bTalking*/);

/**
 * How one player's voice sounds on this machine (S4-07, ADR-0025): a talker on the player's state, on every machine
 * but a dedicated server. A living ExoFrame speaks from its helmet (3D, DA_VoiceConfig's attenuation, the helmet's
 * filter; the rooms' acoustics add reverb and muffle it through walls); without a living body - dead, spectating,
 * the front end - the voice is on the radio (2D, the radio's filter). The engine applies a change from the next word.
 */
UCLASS(ClassGroup = Hollow)
class HOLLOWGAME_API UHollowVoiceTalker : public UVOIPTalker
{
	GENERATED_BODY()

public:
	explicit UHollowVoiceTalker(const FObjectInitializer& ObjectInitializer);

	/** Registers with the owning player state once it has a unique net id (call again whenever it gets one). */
	void TryRegister();

	/** Follows the player's body: from its helmet while it lives, on the radio otherwise. */
	void RefreshPlacement();

	bool IsTalking() const { return bTalking; }
	bool IsOnRadio() const { return bOnRadio; }
	bool IsRegisteredForVoice() const { return bRegisteredForVoice; }

	/** Every talker of this machine. */
	static FOnHollowTalkingChanged OnTalkingChanged;

	virtual void OnTalkingBegin(UAudioComponent* AudioComponent) override;
	virtual void OnTalkingEnd() override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandlePawnSet(APlayerState* Player, APawn* NewPawn, APawn* OldPawn);

	bool bTalking = false;
	bool bOnRadio = true;
	bool bRegisteredForVoice = false;
};
