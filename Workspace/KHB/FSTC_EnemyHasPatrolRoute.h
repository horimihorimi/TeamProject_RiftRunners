#pragma once

#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "FSTC_EnemyHasPatrolRoute.generated.h"

USTRUCT()
struct FSTC_EnemyHasPatrolRouteInstanceData
{
	GENERATED_BODY()

	/** 끄면 "순찰 경로가 없을 때" 참이 된다. 제자리 대기 상태의 진입 조건으로 쓴다. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bShouldHaveRoute = true;
};

/**
 * 스폰한 쪽이 순찰 경로를 넘겨줬는지 판정합니다.
 *
 * 같은 몬스터라도 배치마다 순찰 여부가 다르다. 그래서 Idle 상태가 이 조건으로
 * 순찰과 제자리 대기 중 하나를 고른다. StateTree는 몬스터 종류를 알 필요가 없다.
 */
USTRUCT(meta = (DisplayName = "Enemy Has Patrol Route", Category = "Enemy Framework"))
struct TEAMPROJECT_FIN_API FSTC_EnemyHasPatrolRoute : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSTC_EnemyHasPatrolRouteInstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
