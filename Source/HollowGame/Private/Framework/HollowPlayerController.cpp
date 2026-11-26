#include "Framework/HollowPlayerController.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Classes/HollowClassComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/HollowRaidGameMode.h"
#include "HollowGameLog.h"
#include "Hub/HollowHubStateComponent.h"
#include "Hub/HollowHubTerminal.h"
#include "Hub/HollowHubTerminalSubsystem.h"
#include "InputMappingContext.h"
#include "Profile/HollowProfileSubsystem.h"
#include "Raid/HollowRaidFlowComponent.h"
#include "Raid/HollowRaidLauncher.h"
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
	if (IsLocalController())
	{
		if (UHollowProfileSubsystem* Profile = UHollowProfileSubsystem::Get(this))
		{
			ProfileHandle = Profile->OnProfileChanged.AddUObject(this, &ThisClass::SyncClassWithProfile);
		}
		SyncClassWithProfile();
	}
}

void AHollowPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UHollowProfileSubsystem* Profile = UHollowProfileSubsystem::Get(this))
	{
		Profile->OnProfileChanged.Remove(ProfileHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void AHollowPlayerController::SyncClassWithProfile()
{
	const UHollowProfileSubsystem* Profile = UHollowProfileSubsystem::Get(this);
	if (!Profile || !IsLocalController() || !HollowRaidLauncher::IsInHub(GetWorld()))
	{
		return;   // a raid takes the class from the loadout snapshot
	}
	const FPrimaryAssetId& Class = Profile->GetProfile().ClassId;
	if (Class.IsValid() && Class != RequestedClass)
	{
		RequestedClass = Class;
		Server_SelectClass(Class);
	}
}

void AHollowPlayerController::Client_AcousticAlert_Implementation(const FGameplayTag& NoiseTag)
{
	++AcousticAlerts;
	UE_LOG(LogHollowGame, Log, TEXT("%s: acoustic alert, its %s answered"), *GetName(), *NoiseTag.ToString());
	OnAcousticAlert.Broadcast(NoiseTag);
}

void AHollowPlayerController::Server_SelectClass_Implementation(const FPrimaryAssetId& Class)
{
	UHollowClassComponent* ClassComponent = UHollowClassComponent::Get(PlayerState);
	if (!ClassComponent || !HollowRaidLauncher::IsInHub(GetWorld()))
	{
		UE_LOG(LogHollowGame, Warning, TEXT("Class: %s may not change to %s here: only in the hub"), *GetName(), *Class.ToString());
		return;
	}
	ClassComponent->SetClass(Class);   // refuses (and says so) anything that is not a class
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
		Loadout.Upgrades = Profile->GetProfile().Upgrades;   // the ship's free items come on top (ADR-0032)
		Loadout.ClassId = Profile->GetProfile().ClassId;     // the class played (ADR-0034)
		Loadout.KnownLore = Profile->GetProfile().Lore;      // NAR-01b: which crew logs the squad's datapads hold
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

void AHollowPlayerController::Client_OpenTerminal_Implementation(AHollowHubTerminal* Terminal)
{
	if (UHollowHubTerminalSubsystem* Terminals = UHollowHubTerminalSubsystem::Get(GetWorld()))
	{
		Terminals->Open(this, Terminal);
	}
}

void AHollowPlayerController::Server_SelectContract_Implementation(const FPrimaryAssetId& Contract)
{
	UHollowHubStateComponent* Hub = UHollowHubStateComponent::Get(GetWorld());
	if (!IsLocalController() || !Hub)
	{
		UE_LOG(LogHollowGame, Warning, TEXT("Hub: %s may not choose the contract (%s): only the host does"), *GetName(), *Contract.ToString());
		return;
	}
	Hub->SelectContract(Contract);
}
