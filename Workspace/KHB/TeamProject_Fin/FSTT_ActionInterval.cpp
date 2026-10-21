#include "FSTT_ActionInterval.h"

#include "GameCore/AI/MonsterAIRandom.h"
#include "StateTreeExecutionContext.h"

FSTT_ActionInterval::FSTT_ActionInterval()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FSTT_ActionInterval::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const float MinInterval =
		FMath::Max(0.0f, FMath::Min(InstanceData.MinInterval, InstanceData.MaxInterval));
	const float MaxInterval =
		FMath::Max(0.0f, FMath::Max(InstanceData.MinInterval, InstanceData.MaxInterval));

	InstanceData.ResolvedInterval =
		FMath::IsNearlyEqual(MinInterval, MaxInterval)
		? MinInterval
		: MonsterAIRandom::RandomRange(MinInterval, MaxInterval);

	InstanceData.ElapsedTime = 0.0f;

	return InstanceData.ResolvedInterval <= 0.0f
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FSTT_ActionInterval::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	InstanceData.ElapsedTime += FMath::Max(0.0f, DeltaTime);

	return InstanceData.ElapsedTime >= InstanceData.ResolvedInterval
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Running;
}
