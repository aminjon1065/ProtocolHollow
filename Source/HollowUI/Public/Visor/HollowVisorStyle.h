#pragma once

#include "Engine/DataAsset.h"
#include "Resonance/HollowResonanceTypes.h"
#include "Visor/HollowObjectiveRules.h"
#include "Voice/HollowVoiceRules.h"

#include "HollowVisorStyle.generated.h"

/** How the visor shows one squad Resonance tier: its readout and the glow of the glass. */
USTRUCT(BlueprintType)
struct FHollowVisorTierLook
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visor")
	FText Label;

	/** The readout's colour and the glass's glow. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visor")
	FLinearColor Color = FLinearColor::White;

	/** How strongly the glass glows at its edges (0 = calm glass). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visor", meta = (ClampMin = "0", ClampMax = "1"))
	float Alert = 0.f;

	/** The glow's pulse (0 = steady); never faster than UHollowVisorStyle::MaxPulseHz. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visor", meta = (ClampMin = "0", Units = "Hertz"))
	float PulseHz = 0.f;

	/** Interference bands drifting over the glass. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visor", meta = (ClampMin = "0", ClampMax = "1"))
	float Interference = 0.f;
};

/** How the visor marks one kind of place (UX-01): its label and colour. */
USTRUCT(BlueprintType)
struct FHollowWaypointLook
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waypoints")
	EHollowWaypointKind Kind = EHollowWaypointKind::Extraction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waypoints")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waypoints")
	FLinearColor Color = FLinearColor::White;
};

/**
 * The look of the diegetic visor (S4-08, ADR-0026; DA_VisorStyle, chosen in UHollowUISettings): the Resonance tiers,
 * the telemetry's texts and colours. Layout is W_Visor; the glass is M_UI_VisorGlass.
 */
UCLASS(BlueprintType)
class HOLLOWUI_API UHollowVisorStyle : public UDataAsset
{
	GENERATED_BODY()

public:
	/** No light on the visor pulses faster: three flashes per second is the photosensitive seizure limit (WCAG 2.3.1). */
	static constexpr float MaxPulseHz = 3.f;

	/** The waypoints follow the view at least this often (a marker lagging behind a turn misleads) and at most this often (UX-01). */
	static constexpr float MinWaypointRateHz = 1.f;
	static constexpr float MaxWaypointRateHz = 30.f;

	/** The look of Tier; a calm look if the data lacks it. */
	const FHollowVisorTierLook& GetTierLook(EHollowResonanceTier Tier) const;

	/** The label of a voice mode. */
	const FText& GetVoiceModeLabel(EHollowVoiceMode Mode) const;

	/** How Kind is marked; null if the data lacks it. */
	const FHollowWaypointLook* GetWaypointLook(EHollowWaypointKind Kind) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/** One per tier, Low first (EHollowResonanceTier order). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resonance")
	TArray<FHollowVisorTierLook> Tiers;

	/** {0} = the squad's Resonance in percent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resonance")
	FText ResonanceFormat;

	/** Calm telemetry text. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Telemetry")
	FLinearColor TelemetryColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	FText PushToTalkLabel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	FText OpenMicLabel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	FText MutedLabel;

	/** {0} = the mode's label, while the voice goes out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	FText TransmittingFormat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	FLinearColor TransmittingColor = FLinearColor::Green;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	FLinearColor MutedColor = FLinearColor::Gray;

	/** {0} = who else is talking ("Ann, Bob"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	FText TalkingFormat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lamp")
	FText LampOnLabel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lamp")
	FText LampOffLabel;

	/** {0} = the lamp's key on the current device, {1} = LampOnLabel or LampOffLabel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lamp")
	FText LampFormat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lamp")
	FLinearColor LampOnColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lamp")
	FLinearColor LampOffColor = FLinearColor::Gray;

	/** One consumable of the raid inventory (S5-01): {0} = its name, {1} = how many are left. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supplies")
	FText SupplyFormat;

	/** Between two consumables. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supplies")
	FText SupplySeparator;

	/** UX-01 (ADR-0042): the tracker's title over the objective. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	FText ObjectiveTitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	FLinearColor ObjectiveColor = FLinearColor::White;

	/** One per EHollowWaypointKind. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waypoints")
	TArray<FHollowWaypointLook> Waypoints;

	/** {0} = the place's label, {1} = whole metres to it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waypoints")
	FText WaypointFormat;

	/** How often the markers follow the view while any is shown (a timer, not Tick). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waypoints", meta = (ClampMin = "1", ClampMax = "30", Units = "Hertz"))
	float WaypointRateHz = 12.f;

	/** A place this close is not marked (the player stands at it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waypoints", meta = (ClampMin = "0", Units = "Meters"))
	float WaypointHideMetres = 0.f;

	/** A marker off screen sits this far inside the visor's edges, with an arrow. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waypoints", meta = (ClampMin = "0"))
	float WaypointEdgeMargin = 0.f;

	/** The cheat sheet's title: {0} = the key that folds it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
	FText ControlsTitleFormat;

	/** The title without a key (no local player). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
	FText ControlsTitle;
};
