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
 * StateTree nodes of Hollow enemies (ADR-0017, ADR-0019). The trees (ST_IronPilgrim, ST_EchoStalker, built by Python) only
 * decide: enter conditions read the controller's facts, and each state's task runs one controller activity and
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

USTRUCT(meta = (DisplayName = "Hollow Stunned", Category = "Hollow|AI"))
struct HOLLOWAI_API FHollowSTTask_Stunned : public FHollowSTTask_Activity
{
	GENERATED_BODY()

protected:
	virtual EHollowAIActivity GetActivity() const override { return EHollowAIActivity::Stunned; }
};

USTRUCT(meta = (DisplayName = "Hollow Track Squad", Category = "Hollow|AI"))
struct HOLLOWAI_API FHollowSTTask_Track : public FHollowSTTask_Activity
{
	GENERATED_BODY()

protected:
	virtual EHollowAIActivity GetActivity() const override { return EHollowAIActivity::Track; }
};

USTRUCT(meta = (DisplayName = "Hollow Ambush", Category = "Hollow|AI"))
struct HOLLOWAI_API FHollowSTTask_Ambush : public FHollowSTTask_Activity
{
	GENERATED_BODY()

protected:
	virtual EHollowAIActivity GetActivity() const override { return EHollowAIActivity::Ambush; }
};

USTRUCT(meta = (DisplayName = "Hollow Retreat To Vents", Category = "Hollow|AI"))
struct HOLLOWAI_API FHollowSTTask_Retreat : public FHollowSTTask_Activity
{
	GENERATED_BODY()

protected:
	virtual EHollowAIActivity GetActivity() const override { return EHollowAIActivity::Retreat; }
};

USTRUCT(meta = (DisplayName = "Hollow Recoil From Light", Category = "Hollow|AI"))
struct HOLLOWAI_API FHollowSTTask_Recoil : public FHollowSTTask_Activity
{
	GENERATED_BODY()

protected:
	virtual EHollowAIActivity GetActivity() const override { return EHollowAIActivity::Recoil; }
};

USTRUCT(meta = (DisplayName = "Hollow Flank Through The Dark", Category = "Hollow|AI"))
struct HOLLOWAI_API FHollowSTTask_Flank : public FHollowSTTask_Activity
{
	GENERATED_BODY()

protected:
	virtual EHollowAIActivity GetActivity() const override { return EHollowAIActivity::Flank; }
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

USTRUCT(meta = (DisplayName = "Hollow Is Stunned", Category = "Hollow|AI"))
struct HOLLOWAI_API FHollowSTCond_IsStunned : public FHollowSTCondition_Fact
{
	GENERATED_BODY()

protected:
	virtual bool Test(const AHollowAIController& Controller) const override;
};

USTRUCT(meta = (DisplayName = "Hollow Is Hunting (director)", Category = "Hollow|AI"))
struct HOLLOWAI_API FHollowSTCond_IsHunting : public FHollowSTCondition_Fact
{
	GENERATED_BODY()

protected:
	virtual bool Test(const AHollowAIController& Controller) const override;
};

USTRUCT(meta = (DisplayName = "Hollow Is Retreating (director)", Category = "Hollow|AI"))
struct HOLLOWAI_API FHollowSTCond_IsRetreating : public FHollowSTCondition_Fact
{
	GENERATED_BODY()

protected:
	virtual bool Test(const AHollowAIController& Controller) const override;
};

/** Whisperer (GP-02): a beam caught it and it has not flinched from it yet. */
USTRUCT(meta = (DisplayName = "Hollow Should Recoil (whisperer)", Category = "Hollow|AI"))
struct HOLLOWAI_API FHollowSTCond_ShouldRecoil : public FHollowSTCondition_Fact
{
	GENERATED_BODY()

protected:
	virtual bool Test(const AHollowAIController& Controller) const override;
};

/** Whisperer: its turn to bite (the swarm's bite slots); any other enemy may always strike. */
USTRUCT(meta = (DisplayName = "Hollow Can Bite (whisperer)", Category = "Hollow|AI"))
struct HOLLOWAI_API FHollowSTCond_CanBite : public FHollowSTCondition_Fact
{
	GENERATED_BODY()

protected:
	virtual bool Test(const AHollowAIController& Controller) const override;
};

/** Whisperer: its prey has a light (no frenzy, no lunge under way), or it is not its turn to bite. */
USTRUCT(meta = (DisplayName = "Hollow Should Flank (whisperer)", Category = "Hollow|AI"))
struct HOLLOWAI_API FHollowSTCond_ShouldFlank : public FHollowSTCondition_Fact
{
	GENERATED_BODY()

protected:
	virtual bool Test(const AHollowAIController& Controller) const override;
};
