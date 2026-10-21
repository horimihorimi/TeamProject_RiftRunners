#include "FSTT_SetSelectedAction.h"

#include "GameCore/AI/MonsterAIController.h"
#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FSTT_SetSelectedAction::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AMonsterAIController* Controller = Cast<AMonsterAIController>(Context.GetOwner());

	if (!IsValid(Controller) || !InstanceData.Action.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	Controller->SetSelectedAction(InstanceData.Action);
	return EStateTreeRunStatus::Succeeded;
}
