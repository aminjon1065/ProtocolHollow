#pragma once

#include "Engine/DeveloperSettings.h"

#include "HollowNoiseSettings.generated.h"

class UHollowNoiseConfig;

/** Which noise config the game uses (DefaultGame.ini); the numbers live in the config asset. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Hollow Noise"))
class UHollowNoiseSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Noise")
	TSoftObjectPtr<UHollowNoiseConfig> Config;
};
