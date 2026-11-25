#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "HollowRadioScript.generated.h"

/** Which transmission goes first (NAR-01a, ADR-0049). Only Critical cuts into one already on the air. */
UENUM(BlueprintType)
enum class EHollowRadioPriority : uint8
{
	Low,
	Normal,
	High,
	Critical
};

/** How often a cue is heard. */
UENUM(BlueprintType)
enum class EHollowRadioScope : uint8
{
	/** Once for good: remembered in the player's profile when it has been heard to its end (the hub briefing). */
	Profile,
	/** Once per raid (and once per hub visit). */
	Raid,
	/** Every time its trigger fires, but never again within its cooldown. */
	Repeat
};

/** Who speaks a line. */
UENUM(BlueprintType)
enum class EHollowRadioSpeaker : uint8
{
	/** Vector-9, Helios-Nadir's orbital control. */
	Handler,
	/** The Echo-Stalker mimicking the handler's voice (a corrupted transmission). */
	Echo
};

/** One subtitle on the visor. */
USTRUCT(BlueprintType)
struct HOLLOWUI_API FHollowRadioLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio")
	EHollowRadioSpeaker Speaker = EHollowRadioSpeaker::Handler;

	/** An ST_Radio entry (ADR-0048). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio", meta = (MultiLine = true))
	FText Text;
};

/** One way of saying a cue: its lines, in order. */
USTRUCT(BlueprintType)
struct HOLLOWUI_API FHollowRadioVariant
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio")
	TArray<FHollowRadioLine> Lines;
};

/** What the handler says when a trigger fires. */
USTRUCT(BlueprintType)
struct HOLLOWUI_API FHollowRadioCue
{
	GENERATED_BODY()

	/** Stable id: a Profile cue is remembered by it in the player's profile. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio", meta = (Categories = "Hollow.Radio"))
	FGameplayTag Trigger;

	/** Only in this sector Game Feature (the manifest's SectorPlugin, e.g. HollowSector_Docks); none: anywhere. A cue of the sector goes before one for anywhere. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio")
	FName Sector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio")
	EHollowRadioPriority Priority = EHollowRadioPriority::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio")
	EHollowRadioScope Scope = EHollowRadioScope::Raid;

	/** Repeat: not heard again before this long after it started. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio", meta = (ClampMin = "0", Units = "Seconds"))
	float CooldownSeconds = 0.f;

	/** Waiting for the air longer than this, it is dropped (0: it waits as long as it takes). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio", meta = (ClampMin = "0", Units = "Seconds"))
	float StaleSeconds = 0.f;

	/** One is picked per raid (and per play of a Repeat cue), from the raid's id: never random. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio")
	TArray<FHollowRadioVariant> Variants;
};

/**
 * The handler's radio (NAR-01a, ADR-0049): every cue, its trigger and lines, and how subtitles are paced. One asset,
 * DA_RadioScript (create_story_assets.py), named by UHollowUISettings::RadioScript. Each machine plays it alone from
 * what it already knows: no transmission crosses the network, every player reads it in their own language.
 */
UCLASS(BlueprintType)
class HOLLOWUI_API UHollowRadioScript : public UDataAsset
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/** The cue Trigger starts in Sector: the sector's own first, else one for anywhere; INDEX_NONE without one. */
	int32 FindCue(const FGameplayTag& Trigger, FName Sector) const;

	const FText& GetSpeakerName(EHollowRadioSpeaker Speaker) const;

	/** How long Text stays on the visor: its length at CharactersPerSecond, within MinLineSeconds..MaxLineSeconds. */
	float GetLineSeconds(const FText& Text) const;

	/** "VECTOR-9". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speakers")
	FText HandlerName;

	/** The corrupted call sign the Echo-Stalker uses. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speakers")
	FText EchoName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cues")
	TArray<FHollowRadioCue> Cues;

	/** Reading pace of a subtitle (the text's length in the language shown). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pacing", meta = (ClampMin = "1"))
	float CharactersPerSecond = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pacing", meta = (ClampMin = "0.5", Units = "Seconds"))
	float MinLineSeconds = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pacing", meta = (ClampMin = "0.5", Units = "Seconds"))
	float MaxLineSeconds = 9.f;

	/** Silence between two transmissions (a Critical one does not wait). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pacing", meta = (ClampMin = "0", Units = "Seconds"))
	float CueGapSeconds = 1.5f;

	/** Transmissions waiting at most; a more urgent one pushes out the least urgent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pacing", meta = (ClampMin = "1"))
	int32 MaxQueued = 4;

	/** How often the helmet looks for a core in its vault and for the Echo-Stalker while either is still unseen (a timer). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sensors", meta = (ClampMin = "0.1", Units = "Seconds"))
	float SensorIntervalSeconds = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sensors", meta = (ClampMin = "0", Units = "Centimeters"))
	float CoreSightDistance = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sensors", meta = (ClampMin = "0", Units = "Centimeters"))
	float StalkerSightDistance = 2000.f;

	/** Half the angle of the view in which a sensor counts a thing as seen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sensors", meta = (ClampMin = "1", ClampMax = "90", Units = "Degrees"))
	float SightHalfAngleDegrees = 45.f;
};
