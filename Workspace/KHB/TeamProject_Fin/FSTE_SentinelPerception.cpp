#include "FSTE_SentinelPerception.h"

#include "GameCore/AI/MonsterAIController.h"
#include "StateTreeExecutionContext.h"

void FSTE_SentinelPerception::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	AMonsterAIController* Controller =
		Cast<AMonsterAIController>(Context.GetOwner());

	if (!IsValid(Controller))
	{
		return;
	}

	InstanceData.TargetActor = Controller->GetPerceivedTarget();
	InstanceData.bHasTarget = Controller->HasPerceivedTarget();

	Controller->UpdatePerceptionEvaluatorDebug(
		InstanceData.TargetActor,
		InstanceData.bHasTarget);
}
