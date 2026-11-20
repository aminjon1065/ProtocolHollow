#pragma once

#include "Brain/HollowAITypes.h"
#include "StateTreeConditionBase.h"
#include "StateTreeTaskBase.h"

#include "HollowStateTreeNodes.generated.h"

class AHollowAIController;

/** The nodes keep nothing of their own (the controller holds facts and activities), but StateTree requires instance data. */
USTRUCT()
struct HOLLOWAI_API FHollowSTNodeInstanceData
{
	GENERATED_BODY()
};

/**
 * StateTree nodes of Hollow enemies (ADR-0017). The tree (ST_IronPilgrim, built by create_pilgrim_assets.py) only
 * decides: enter conditions read the controller's facts, and each state's task runs one controller activity and
 * finishes when the controller says it is done. Nothing ticks: the tree sleeps until an activity finishes or the
 * controller sends Hollow.AI.Event.Reconsider. The owner of the tree is the AI controller (StateTree AI component).
 */
USTRUCT(meta = (Hidden))
struct HOLLOWAI_API FHollowSTTask_Activity : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FHollowSTNodeInstanceData;

	FHollowSTTask_Activity();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

protected:
	virtual EHollowAIActivity GetActivity() const { return EHollowAIActivity::None; }
};

USTRUCT(meta = (DisplayName = "Hollow Patrol", Category = "Hollow|AI"))
struct HOLLOWAI_API FHollowSTTask_Patrol : public FHollowSTTask_Activity
{
	GENERATED_BODY()

protected:
	virtual EHollowAIActivity GetActivity() const override { return EHollowAIActivity::Patrol; }
};

USTRUCT(meta = (DisplayName = "Hollow Chase", Category = "Hollow|AI"))
struct HOLLOWAI_API FHollowSTTask_Chase : public FHollowSTTask_Activity
{
	GENERATED_BODY()

protected:
	virtual EHollowAIActivity GetActivity() const override { return EHollowAIActivity::Chase; }
};

USTRUCT(meta = (DisplayName = "Hollow Investigate", Category = "Hollow|AI"))
struct HOLLOWAI_API FHollowSTTask_Investigate : public FHollowSTTask_Activity
{
	GENERATED_BODY()

protected:
	virtual EHollowAIActivity GetActivity() const override { return EHollowAIActivity::Investigate; }
};

USTRUCT(meta = (DisplayName = "Hollow Attack", Category = "Hollow|AI"))
struct HOLLOWAI_API FHollowSTTask_Attack : public FHollowSTTask_Activity
{
	GENERATED_BODY()

protected:
	virtual EHollowAIActivity GetActivity() const override { return EHollowAIActivity::Attack; }
};

USTRUCT(meta = (DisplayName = "Hollow Breach Obstacle", Category = "Hollow|AI"))
struct HOLLOWAI_API FHollowSTTask_Breach : public FHollowSTTask_Activity
{
	GENERATED_BODY()

protected:
	virtual EHollowAIActivity GetActivity() const override { return EHollowAIActivity::Breach; }
};

/** Base of the fact conditions: false without a Hollow AI controller as the tree's owner. */
USTRUCT(meta = (Hidden))
struct HOLLOWAI_API FHollowSTCondition_Fact : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FHollowSTNodeInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

protected:
	virtual bool Test(const AHollowAIController& Controller) const { return false; }
};

USTRUCT(meta = (DisplayName = "Hollow Has Target", Category = "Hollow|AI"))
struct HOLLOWAI_API FHollowSTCond_HasTarget : public FHollowSTCondition_Fact
{
	GENERATED_BODY()

protected:
	virtual bool Test(const AHollowAIController& Controller) const override;
};

USTRUCT(meta = (DisplayName = "Hollow Target In Engage Range", Category = "Hollow|AI"))
struct HOLLOWAI_API FHollowSTCond_TargetInEngageRange : public FHollowSTCondition_Fact
{
	GENERATED_BODY()

protected:
	virtual bool Test(const AHollowAIController& Controller) const override;
};

USTRUCT(meta = (DisplayName = "Hollow Has Stimulus", Category = "Hollow|AI"))
struct HOLLOWAI_API FHollowSTCond_HasStimulus : public FHollowSTCondition_Fact
{
	GENERATED_BODY()

protected:
	virtual bool Test(const AHollowAIController& Controller) const override;
};

USTRUCT(meta = (DisplayName = "Hollow Has Obstacle", Category = "Hollow|AI"))
struct HOLLOWAI_API FHollowSTCond_HasObstacle : public FHollowSTCondition_Fact
{
	GENERATED_BODY()

protected:
	virtual bool Test(const AHollowAIController& Controller) const override;
};
