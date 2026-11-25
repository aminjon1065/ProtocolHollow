#include "HollowGameLog.h"

#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "Settings/HollowGameUserSettings.h"

DEFINE_LOG_CATEGORY(LogHollowGame);

/** The game module: once the engine is up, a game (not the editor, a commandlet or a dedicated server) applies its language and graphics preset. */
class FHollowGameModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		PostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddLambda([]
		{
			if (GIsEditor || IsRunningCommandlet() || IsRunningDedicatedServer())
			{
				return;
			}
			if (UHollowGameUserSettings* Settings = UHollowGameUserSettings::Get())
			{
				Settings->InitializeLanguage();   // before any screen shows a text (LOC-01)
				Settings->InitializePreset();
			}
		});
	}

	virtual void ShutdownModule() override
	{
		FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);
	}

private:
	FDelegateHandle PostEngineInitHandle;
};

IMPLEMENT_PRIMARY_GAME_MODULE(FHollowGameModule, HollowGame, "ProtocolHollow");
