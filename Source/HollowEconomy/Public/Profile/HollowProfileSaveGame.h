#pragma once

#include "GameFramework/SaveGame.h"
#include "Profile/HollowProfileTypes.h"

#include "HollowProfileSaveGame.generated.h"

/** The local profile on disk (ADR-0005; Steam Auto-Cloud syncs the save folder, no code). */
UCLASS()
class HOLLOWECONOMY_API UHollowProfileSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** Bumped with every incompatible change of FHollowProfileState. */
	static constexpr int32 CurrentVersion = 1;

	UPROPERTY()
	int32 Version = CurrentVersion;

	UPROPERTY()
	FHollowProfileState State;
};
