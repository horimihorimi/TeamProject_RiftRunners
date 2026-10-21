#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "FSTT_EnemyReturnToDormant.generated.h"

USTRUCT()
struct FSTT_EnemyReturnToDormantInstanceData
{
	GENERATED_BODY()
};

/**
 * 타깃을 잃은 적을 다시 휴면 상태로 되돌립니다.
 *
 * 휴면 태그가 붙으면 컨트롤러가 시야 평가를 멈춘다(ACommonEnemyAIController).
 * 다시 조우하기 전까지는 플레이어를 인지하지 않는다.
 *
 * 표시와 충돌은 건드리지 않는다. 등장 연출을 되감는 일은 조우 시스템의 몫이다.
 */
USTRUCT(meta = (DisplayName = "Enemy Return To Dormant", Category = "Enemy Framework"))
struct TEAMPROJECT_FIN_API FSTT_EnemyReturnToDormant : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSTT_EnemyReturnToDormantInstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};
