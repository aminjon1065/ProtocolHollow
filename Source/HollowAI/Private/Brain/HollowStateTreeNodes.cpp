#include "Brain/HollowStateTreeNodes.h"

#include "Brain/HollowAIController.h"
#include "Brain/HollowStalkerController.h"
#include "Brain/HollowWhispererController.h"
#include "StateTreeAsyncExecutionContext.h"
#include "StateTreeExecutionContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowStateTreeNodes)

FHollowSTTask_Activity::FHollowSTTask_Activity()
{
	bShouldCallTick = false;   // finished by the controller's callback, so the tree can sleep
}

EStateTreeRunStatus FHollowSTTask_Activity::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	AHollowAIController* Controller = Cast<AHollowAIController>(Context.GetOwner());
	if (!Controller)
	{
		return EStateTreeRunStatus::Failed;
	}
	Controller->BeginActivity(GetActivity(), FOnHollowActivityFinished::CreateLambda([WeakContext = Context.MakeWeakExecutionContext()](const bool bSuccess)
	{
		WeakContext.FinishTask(bSuccess ? EStateTreeFinishTaskType::Succeeded : EStateTreeFinishTaskType::Failed);
	}));
	return EStateTreeRunStatus::Running;
}

void FHollowSTTask_Activity::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (AHollowAIController* Controller = Cast<AHollowAIController>(Context.GetOwner()); Controller && Controller->GetActivity() == GetActivity())
	{
		Controller->EndActivity();
	}
}

bool FHollowSTCondition_Fact::TestCondition(FStateTreeExecutionContext& Context) const
{
	const AHollowAIController* Controller = Cast<AHollowAIController>(Context.GetOwner());
	return Controller && Test(*Controller);
}

bool FHollowSTCond_HasTarget::Test(const AHollowAIController& Controller) const
{
	return Controller.GetTarget() != nullptr;
}

bool FHollowSTCond_TargetInEngageRange::Test(const AHollowAIController& Controller) const
{
	return Controller.IsTargetInEngageRange();
}

bool FHollowSTCond_HasStimulus::Test(const AHollowAIController& Controller) const
{
	return Controller.HasStimulus();
}

bool FHollowSTCond_HasObstacle::Test(const AHollowAIController& Controller) const
{
	return Controller.GetObstacle() != nullptr;
}

bool FHollowSTCond_IsStunned::Test(const AHollowAIController& Controller) const
{
	return Controller.IsStunned();
}

bool FHollowSTCond_IsHunting::Test(const AHollowAIController& Controller) const
{
	const AHollowStalkerController* Stalker = Cast<AHollowStalkerController>(&Controller);
	return Stalker && Stalker->IsHunting();
}

bool FHollowSTCond_IsRetreating::Test(const AHollowAIController& Controller) const
{
	const AHollowStalkerController* Stalker = Cast<AHollowStalkerController>(&Controller);
	return Stalker && Stalker->IsRetreating();
}

bool FHollowSTCond_ShouldRecoil::Test(const AHollowAIController& Controller) const
{
	const AHollowWhispererController* Whisperer = Cast<AHollowWhispererController>(&Controller);
	return Whisperer && Whisperer->ShouldRecoil();
}

bool FHollowSTCond_CanBite::Test(const AHollowAIController& Controller) const
{
	const AHollowWhispererController* Whisperer = Cast<AHollowWhispererController>(&Controller);
	return !Whisperer || Whisperer->CanBite();
}

bool FHollowSTCond_ShouldFlank::Test(const AHollowAIController& Controller) const
{
	const AHollowWhispererController* Whisperer = Cast<AHollowWhispererController>(&Controller);
	return Whisperer && Whisperer->ShouldFlank();
}
