#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "FSTT_EnemySearch.generated.h"

USTRUCT()
struct FSTT_EnemySearchInstanceData
{
	GENERATED_BODY()

	/** 탐색이 끝나는 월드 시간. */
	double SearchEndTime = 0.0;
};

/**
 * 타깃을 잃은 뒤 제자리에서 정해진 시간 동안 탐색합니다.
 *
 * 시간이 다 되면 성공으로 끝난다. 돌아갈 곳은 이 태스크가 정하지 않고,
 * 이 상태를 빠져나간 StateTree가 Idle로 보내 순찰 여부를 다시 고르게 한다.
 *
 * 탐색 도중 타깃을 다시 찾는 경우는 상위 상태의 전환 조건이 처리한다.
 */
USTRUCT(meta = (DisplayName = "Enemy Search", Category = "Enemy Framework"))
struct TEAMPROJECT_FIN_API FSTT_EnemySearch : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSTT_EnemySearchInstanceData;

	FSTT_EnemySearch();

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
	virtual void ExitState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};
