#include "Framework/HollowPlayerController.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/HollowRaidGameMode.h"
#include "HollowGameLog.h"
#include "InputMappingContext.h"
#include "Profile/HollowProfileSubsystem.h"
#include "Raid/HollowRaidFlowComponent.h"
#include "Sector/HollowSectorAuthorityComponent.h"
#include "Util/HollowEnumUtils.h"
#include "Voice/HollowVoiceComponent.h"
#include "Voice/HollowVoiceConfig.h"
#include "Voice/HollowVoiceSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowPlayerController)

AHollowPlayerController::AHollowPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Voice = CreateDefaultSubobject<UHollowVoiceComponent>(TEXT("Voice"));
}

void AHollowPlayerController::BeginPlay()
{
	Super::BeginPlay();
	const UHollowVoiceConfig* Config = GetDefault<UHollowVoiceSettings>()->LoadConfig();
	UEnhancedInputLocalPlayerSubsystem* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (Input && Config && Config->MappingContext)
	{
		Input->AddMappingContext(Config->MappingContext, Config->MappingPriority);   // V / LB, whatever the pawn
	}
}

void AHollowPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
	const UHollowVoiceConfig* Config = GetDefault<UHollowVoiceSettings>()->LoadConfig();
	if (!Input || !Config || !Config->PushToTalkAction)
	{
		UE_LOG(LogHollowGame, Warning, TEXT("%s: no push-to-talk (Hollow Voice settings: VoiceConfig, or DA_VoiceConfig's PushToTalkAction)."), *GetName());
		return;
	}
	Input->BindAction(Config->PushToTalkAction, ETriggerEvent::Started, this, &ThisClass::HandlePushToTalkPressed);
	Input->BindAction(Config->PushToTalkAction, ETriggerEvent::Completed, this, &ThisClass::HandlePushToTalkReleased);
	Input->BindAction(Config->PushToTalkAction, ETriggerEvent::Canceled, this, &ThisClass::HandlePushToTalkReleased);
}

void AHollowPlayerController::HandlePushToTalkPressed()
{
	Voice->SetPushToTalk(true);
}

void AHollowPlayerController::HandlePushToTalkReleased()
{
	Voice->SetPushToTalk(false);
}

void AHollowPlayerController::Server_ReportSectorReady_Implementation(const int32 BuildId, const uint32 LayoutHash)
{
	const AHollowRaidGameMode* Raid = GetWorld()->GetAuthGameMode<AHollowRaidGameMode>();
	if (!Raid)
	{
		UE_LOG(LogHollowGame, Verbose, TEXT("%s reported sector build %d outside a raid; ignored."), *GetName(), BuildId);
		return;
	}
	Raid->GetSectorAuthority()->HandleReport(this, BuildId, LayoutHash);
}

void AHollowPlayerController::Client_ReceiveRaidResult_Implementation(const FHollowRaidResult& Result)
{
	UE_LOG(LogHollowGame, Log, TEXT("%s: raid %s result %s"), *GetName(), *Result.RaidId.ToString(), *HollowEnumToString(Result.Outcome));
	if (UHollowProfileSubsystem* Profile = UHollowProfileSubsystem::Get(this))
	{
		Profile->ApplyRaidResult(Result);
	}
}

void AHollowPlayerController::Client_RequestLoadout_Implementation(const FGuid& RaidId)
{
	UHollowProfileSubsystem* Profile = UHollowProfileSubsystem::Get(this);
	if (!Profile)
	{
		UE_LOG(LogHollowGame, Warning, TEXT("%s: raid %s asks for the loadout, but this machine has no profile; going in empty-handed"), *GetName(),
			*RaidId.ToString());
	}
	FHollowLoadoutSnapshot Loadout;
	Loadout.RaidId = RaidId;
	if (Profile)
	{
		Loadout.Slots = Profile->GetLoadoutForRaid(RaidId);
	}
	Server_SubmitLoadout(Loadout);
}

void AHollowPlayerController::Server_SubmitLoadout_Implementation(const FHollowLoadoutSnapshot& Loadout)
{
	const AHollowRaidGameMode* Raid = GetWorld()->GetAuthGameMode<AHollowRaidGameMode>();
	if (!Raid)
	{
		UE_LOG(LogHollowGame, Verbose, TEXT("%s sent a loadout outside a raid; ignored."), *GetName());
		return;
	}
	Raid->GetRaidFlow()->AcceptLoadout(this, Loadout);
}
