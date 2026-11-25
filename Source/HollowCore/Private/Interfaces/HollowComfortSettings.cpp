#include "Interfaces/HollowComfortSettings.h"

#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowComfortSettings)

bool IHollowComfortSettings::IsReducedFlashesChosen()
{
	const IHollowComfortSettings* Settings = GEngine ? Cast<IHollowComfortSettings>(GEngine->GetGameUserSettings()) : nullptr;
	return Settings && Settings->IsReducedFlashes();
}
