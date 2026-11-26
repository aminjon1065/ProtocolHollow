#pragma once

#include "Engine/DataAsset.h"
#include "Hub/HollowHubBadge.h"

#include "HollowUIStyle.generated.h"

class UHollowBadgeWidget;
class USoundBase;

/** A badge kind's colours. */
USTRUCT(BlueprintType)
struct HOLLOWUI_API FHollowBadgeLook
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Badge")
	EHollowHubBadgeKind Kind = EHollowHubBadgeKind::Price;

	/** The badge's glass and its frame. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Badge")
	FLinearColor Fill = FLinearColor::Black;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Badge")
	FLinearColor Frame = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Badge")
	FLinearColor Text = FLinearColor::White;
};

/**
 * The menus' and hub screens' look at run time (DA_UIStyle, UI-02, ADR-0052): badge colours, how a refused button
 * greys out, the terminal screens' sound. The rest of the techno-occult style (glass, frames, type) is baked into the
 * widget trees by the content scripts (Build/Python/hollow_ui_style.py).
 */
UCLASS(BlueprintType)
class HOLLOWUI_API UHollowUIStyle : public UDataAsset
{
	GENERATED_BODY()

public:
	/** The style UHollowUISettings names (a small asset: loaded on first use), or null. */
	static const UHollowUIStyle* Get();

	const FHollowBadgeLook* FindBadge(EHollowHubBadgeKind Kind) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Badges", meta = (TitleProperty = "Kind"))
	TArray<FHollowBadgeLook> Badges;

	/** W_HubBadge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Badges")
	TSoftClassPtr<UHollowBadgeWidget> BadgeClass;

	/** A refused button's opacity: it keeps the focus, its press is refused with the deny sound and its reason. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buttons", meta = (ClampMin = "0.1", ClampMax = "1"))
	float RefusedOpacity = 1.f;

	/** Plays when a hub terminal's screen opens; pitched down when it closes (0 WAV: a MetaSound). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TSoftObjectPtr<USoundBase> TerminalSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds", meta = (ClampMin = "0.25", ClampMax = "2"))
	float TerminalClosePitch = 1.f;
};
