#pragma once

#include "Components/ActorComponent.h"
#include "Damage/HollowDamageMath.h"
#include "GameplayTagContainer.h"

#include "HollowHandsComponent.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
class UHollowWeaponDefinition;
struct FStreamableHandle;

/** What the hands hold (GP-01, ADR-0044). */
UENUM(BlueprintType)
enum class EHollowHandsMode : uint8
{
	/** The loadout's tools on the primary and secondary inputs (cutter or lantern, welder). */
	Tools,
	/** The loadout's weapon: the primary input fires, the secondary aims, the reload input reloads it. */
	Weapon
};

/** What a hand input asks for. */
UENUM()
enum class EHollowHandsRequest : uint8
{
	Tools,
	Weapon,
	/** The other one. */
	Cycle
};

/** The hands switched (any machine: the owner when it predicts, the server, everyone else on replication). */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnHollowHandsChanged, EHollowHandsMode /*Mode*/);

/** The owner's shot or blow hit something, as the owner sees it (GP-01c: the visor's hit marker). */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnHollowHitMarker, EHollowHitKind /*Kind*/);

/**
 * The hands of an ExoFrame (GP-01, ADR-0044): tools or the weapon. GA_SwapHands switches them, predicted by the owner
 * (Mode replicates to everyone else, push model); the weapon is the server's, from the raid inventory (the fire ability
 * it granted), and replicates to everyone (the body shows it). On every machine the hands keep the loose tag
 * Hollow.State.Hands.Weapon on the ability system while the weapon is in hand: the weapon abilities need it, the tools
 * are blocked by it, the same on the server and the owner. Shows the held item through UHollowToolMountComponent and
 * keeps the owner's shot pacing and serials. Never ticks.
 */
UCLASS(ClassGroup = Hollow, meta = (BlueprintSpawnableComponent))
class HOLLOWGAME_API UHollowHandsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	explicit UHollowHandsComponent(const FObjectInitializer& ObjectInitializer);

	static UHollowHandsComponent* Get(const AActor* Owner);

	EHollowHandsMode GetMode() const { return Mode; }
	const UHollowWeaponDefinition* GetWeapon() const { return Weapon; }
	bool IsWeaponInHand() const { return Mode == EHollowHandsMode::Weapon && Weapon != nullptr; }

	/** The mode a request leads to from now: the weapon only if there is one. */
	EHollowHandsMode Resolve(EHollowHandsRequest Request) const;

	/**
	 * Switches the hands on this machine (GA_SwapHands: the predicting owner and the server). The weapon mode needs a
	 * weapon. True if anything changed.
	 */
	bool SetMode(EHollowHandsMode NewMode);

	/**
	 * Server: the weapon of the raid inventory, found among the abilities of the ability system (the fire ability an
	 * item granted); none sends the hands back to the tools. Called on possession and whenever the inventory changes.
	 */
	void RefreshWeapon();

	/** Server: sets the weapon directly (tests, RefreshWeapon). */
	void SetWeapon(const UHollowWeaponDefinition* InWeapon);

	/**
	 * Where a shooter's shots and blows start and go (any machine): the first-person camera along the aim (the control
	 * rotation, never the camera's cosmetic kick) for an ExoFrame, the eyes for any other pawn. The server sees the same
	 * within the weapon's tolerances.
	 */
	static void GetAimView(const APawn& Shooter, FVector& OutOrigin, FVector& OutAim);

	/** The tag on the ability system and the item in hand, to match Mode and Weapon (the pawn calls it when its ability system comes or goes). */
	void Sync();

	/** The ability system whose tag the hands keep (the owning player state's, through the pawn). */
	UAbilitySystemComponent* GetAbilitySystem() const;

	/** Owner: may it fire now (its own pacing: the weapon's exact interval since its last shot)? */
	bool CanFireAt(double Now) const;

	/** Owner: a shot went off now; returns its serial (the seed of its pellets). */
	uint16 NoteShot(double Now);

	/** The last shot's serial on this machine. */
	uint16 GetShotSerial() const { return ShotSerial; }

	/** Server statistics of the owner's hits (Hollow.Weapon.Dump). */
	void CountServerHit(bool bAccepted) { ++(bAccepted ? AcceptedHits : RejectedHits); }
	int32 GetAcceptedHits() const { return AcceptedHits; }
	int32 GetRejectedHits() const { return RejectedHits; }

	FString Describe() const;

	/** Owner: its shot or blow hit (GP-01c); the visor shows the marker. None does nothing. */
	void ShowHitMarker(EHollowHitKind Kind);
	EHollowHitKind GetLastHitMarker() const { return LastHitMarker; }
	int32 GetHitMarkerCount() const { return HitMarkers; }

	FOnHollowHandsChanged OnHandsChanged;
	FOnHollowHitMarker OnHitMarker;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_Mode();

	UFUNCTION()
	void OnRep_Weapon();


	/** Push model, to everyone but the owner (it predicts its own). */
	UPROPERTY(ReplicatedUsing = OnRep_Mode)
	EHollowHandsMode Mode = EHollowHandsMode::Tools;

	/** The server's: what the raid inventory holds. Push model, to everyone. */
	UPROPERTY(ReplicatedUsing = OnRep_Weapon)
	TObjectPtr<const UHollowWeaponDefinition> Weapon;

	/** The weapon's shot effects, loaded while it is carried (the fire cue spawns what is loaded). */
	void LoadWeaponAssets();

	TSharedPtr<FStreamableHandle> WeaponAssets;

	/** Where the loose tag is now (to take it off the same ability system). */
	TWeakObjectPtr<UAbilitySystemComponent> TaggedAbilitySystem;
	bool bTagged = false;

	double LastShotTime = -1.0e9;
	uint16 ShotSerial = 0;
	int32 AcceptedHits = 0;
	int32 RejectedHits = 0;
	EHollowHitKind LastHitMarker = EHollowHitKind::None;
	int32 HitMarkers = 0;
};
