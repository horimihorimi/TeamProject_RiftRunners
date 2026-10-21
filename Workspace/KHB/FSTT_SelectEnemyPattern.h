#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "FSTT_SelectEnemyPattern.generated.h"

/** StateTree가 태스크 노드를 직렬화할 때 필요한 빈 런타임 데이터입니다. */
USTRUCT()
struct FSTT_SelectEnemyPatternInstanceData
{
	GENERATED_BODY()
};

/**
 * EnemyPatternComponent의 Pending Pattern을 승격하거나 새 Current Pattern을 고릅니다.
 *
 * 커밋된 행동(State.AI.ActionCommitted)이 아직 돌고 있으면 끝날 때까지 기다린다.
 * 타깃을 잃어도 계속하는 GA(빙의 등)가 남아 있는데 다음 패턴을 실행하면, 새 몽타주가 그 GA의 몽타주를 끊는다.
 */
USTRUCT(meta = (DisplayName = "Select Enemy Pattern", Category = "Enemy Framework"))
struct TEAMPROJECT_FIN_API FSTT_SelectEnemyPattern : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSTT_SelectEnemyPatternInstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	FSTT_SelectEnemyPattern();

	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;

	virtual EStateTreeRunStatus Tick(
		FStateTreeExecutionContext& Context,
		float DeltaTime) const override;

private:
	/** 커밋된 행동이 없으면 패턴을 고르고, 있으면 Running으로 기다린다. */
	EStateTreeRunStatus TrySelect(FStateTreeExecutionContext& Context) const;
};
