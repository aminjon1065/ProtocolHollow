#pragma once

#include "GameplayEffect.h"

#include "HollowToolOverheatEffect.generated.h"

/**
 * Structure of the tool overheat effect: has a duration and grants Hollow.State.Tool.Overheated, which blocks every
 * continuous tool. The duration is data (GE_Tool_Overheat derives from this).
 */
UCLASS(Abstract)
class HOLLOWGAS_API UHollowToolOverheatEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	explicit UHollowToolOverheatEffect(const FObjectInitializer& ObjectInitializer);
};
