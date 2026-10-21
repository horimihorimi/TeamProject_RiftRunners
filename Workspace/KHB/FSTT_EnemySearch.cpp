#include "FSTT_EnemySearch.h"

#include "Engine/World.h"
#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/Character/CommonEnemyCharacter.h"
#include "GameCore/Movement/MonsterMovementComponent.h"
#include "StateTreeExecutionContext.h"

namespace EnemySearchStateTree
{
	static ACommonEnemyCharacter* GetCharacter(const FStateTreeExecutionContext& Context)
	{
		const AMonsterAIController* Controller = Cast<AMonsterAIController>(Context.GetOwner());
		return IsValid(Controller) ? Cast<ACommonEnemyCharacter>(Controller->GetPawn()) : nullptr;
	}
}

FSTT_EnemySearch::FSTT_EnemySearch()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FSTT_EnemySearch::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const ACommonEnemyCharacter* Character = EnemySearchStateTree::GetCharacter(Context);
	const UWorld* World = IsValid(Character) ? Character->GetWorld() : nullptr;
	if (!IsValid(World))
	{
		return EStateTreeRunStatus::Failed;
	}

	const float SearchDuration = Character->GetSearchDuration();
	if (SearchDuration <= 0.0f)
	{
		// 탐색하지 않는 설정이다. 곧바로 끝내 Idle로 돌아간다.
		return EStateTreeRunStatus::Succeeded;
	}

	InstanceData.SearchEndTime = World->GetTimeSeconds() + SearchDuration;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FSTT_EnemySearch::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const ACommonEnemyCharacter* Character = EnemySearchStateTree::GetCharacter(Context);
	const UWorld* World = IsValid(Character) ? Character->GetWorld() : nullptr;
	if (!IsValid(World))
	{
		return EStateTreeRunStatus::Failed;
	}

	if (World->GetTimeSeconds() >= InstanceData.SearchEndTime)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

void FSTT_EnemySearch::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	ACommonEnemyCharacter* Character = EnemySearchStateTree::GetCharacter(Context);
	if (UMonsterMovementComponent* MovementComponent = IsValid(Character)
		? Cast<UMonsterMovementComponent>(Character->GetCharacterMovement())
		: nullptr)
	{
		MovementComponent->ResetMovementState();
	}
}
