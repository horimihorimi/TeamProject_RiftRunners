#include "FSTC_EnemyHasPatrolArea.h"

#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/Character/CommonEnemyCharacter.h"
#include "StateTreeExecutionContext.h"

bool FSTC_EnemyHasPatrolArea::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const AMonsterAIController* Controller = Cast<AMonsterAIController>(Context.GetOwner());
	const ACommonEnemyCharacter* Character = IsValid(Controller)
		? Cast<ACommonEnemyCharacter>(Controller->GetPawn())
		: nullptr;

	const bool bHasArea = IsValid(Character) && Character->HasPatrolArea();
	return bHasArea == InstanceData.bShouldHaveArea;
}
