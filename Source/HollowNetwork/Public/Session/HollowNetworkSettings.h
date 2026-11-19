#pragma once

#include "Engine/DeveloperSettings.h"

#include "HollowNetworkSettings.generated.h"

class UWorld;

/** Project-wide session settings (Project Settings > Game > Hollow Network, stored in DefaultGame.ini). */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Hollow Network"))
class HOLLOWNETWORK_API UHollowNetworkSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Players per session, host included (brief: 1-4 players). */
	UPROPERTY(Config, EditAnywhere, Category = "Session", meta = (ClampMin = "1", ClampMax = "4"))
	int32 MaxPlayers = 4;

	/** Net update rate of player states, which own the ability system (attributes, effects). 0 keeps the engine default. */
	UPROPERTY(Config, EditAnywhere, Category = "Replication", meta = (ClampMin = "0", Units = "Hertz"))
	float PlayerStateNetUpdateFrequency = 0.f;

	/** Map opened after leaving a session or on startup (front-end). */
	UPROPERTY(Config, EditAnywhere, Category = "Maps")
	TSoftObjectPtr<UWorld> FrontEndMap;

	/** Map opened as a listen server when a session is hosted. */
	UPROPERTY(Config, EditAnywhere, Category = "Maps")
	TSoftObjectPtr<UWorld> HubMap;
};
