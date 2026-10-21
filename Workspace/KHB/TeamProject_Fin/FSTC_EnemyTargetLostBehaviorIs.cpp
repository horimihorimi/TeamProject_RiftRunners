#include "FSTC_EnemyTargetLostBehaviorIs.h"

#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/Character/CommonEnemyCharacter.h"
#include "StateTreeExecutionContext.h"

bool FSTC_EnemyTargetLostBehaviorIs::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const AMonsterAIController* Controller = Cast<AMonsterAIController>(Context.GetOwner());
	const ACommonEnemyCharacter* Character = IsValid(Controller)
		? Cast<ACommonEnemyCharacter>(Controller->GetPawn())
		: nullptr;
	if (!IsValid(Character))
	{
		return false;
	}

	return Character->GetTargetLostBehavior() == InstanceData.Behavior;
}
