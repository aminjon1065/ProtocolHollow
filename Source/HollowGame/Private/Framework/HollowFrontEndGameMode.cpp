#include "Framework/HollowFrontEndGameMode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowFrontEndGameMode)

AHollowFrontEndGameMode::AHollowFrontEndGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DefaultPawnClass = nullptr;   // a menu, not a body
	bStartPlayersAsSpectators = true;
}
