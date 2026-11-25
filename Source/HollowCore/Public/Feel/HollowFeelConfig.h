#pragma once

#include "Engine/DataAsset.h"
#include "Engine/DeveloperSettings.h"
#include "Feel/HollowFeelTypes.h"

#include "HollowFeelConfig.generated.h"

/** Every game feel by trigger (DA_FeelConfig, UHollowFeelSettings::Config; GP-01c, ADR-0046). */
UCLASS(BlueprintType)
class HOLLOWCORE_API UHollowFeelConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** The config named by UHollowFeelSettings (loaded on first use), or null. */
	static const UHollowFeelConfig* Get();

	const FHollowFeelEntry* Find(const FGameplayTag& Trigger) const;

#if WITH_EDITOR
	/** HollowFeelRules::Check. */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel", meta = (TitleProperty = "Trigger"))
	TArray<FHollowFeelEntry> Entries;

	/** With the player's reduced flashes (ADR-0029) every shake and kick is this strong (rumble is kept). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel", meta = (ClampMin = "0", ClampMax = "1"))
	float ReducedFlashesScale = 1.f;

	/** The first-person hands lag behind the view: its turn rate (degrees per second) times this, in degrees... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sway", meta = (ClampMin = "0"))
	float SwayPerTurnRate = 0.f;

	/** ...never more than this far. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sway", meta = (ClampMin = "0", Units = "Degrees"))
	float MaxSway = 0.f;

	/** How fast the hands catch up with the view (the sway's spring; higher is stiffer). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sway", meta = (ClampMin = "0", Units = "Hertz"))
	float SwayReturnRate = 0.f;
};

/** Which feel config the game uses (DefaultGame.ini). */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Hollow Feel"))
class HOLLOWCORE_API UHollowFeelSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Feel")
	TSoftObjectPtr<UHollowFeelConfig> Config;
};
