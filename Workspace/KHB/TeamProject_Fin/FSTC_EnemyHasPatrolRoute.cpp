#include "FSTC_EnemyHasPatrolRoute.h"

#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/Character/CommonEnemyCharacter.h"
#include "StateTreeExecutionContext.h"

bool FSTC_EnemyHasPatrolRoute::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const AMonsterAIController* Controller = Cast<AMonsterAIController>(Context.GetOwner());
	const ACommonEnemyCharacter* Character = IsValid(Controller)
		? Cast<ACommonEnemyCharacter>(Controller->GetPawn())
		: nullptr;

	const bool bHasRoute = IsValid(Character) && Character->HasPatrolRoute();
	return bHasRoute == InstanceData.bShouldHaveRoute;
}
