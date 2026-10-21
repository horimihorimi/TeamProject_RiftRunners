#pragma once

#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "FSTC_EnemyHasPatrolArea.generated.h"

USTRUCT()
struct FSTC_EnemyHasPatrolAreaInstanceData
{
	GENERATED_BODY()

	/** 끄면 "순찰 영역이 없을 때" 참이 된다. 제자리 대기 상태의 진입 조건으로 쓴다. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bShouldHaveArea = true;
};

/**
 * 스폰 포인트가 순찰 영역을 넘겨줬는지 판정합니다.
 *
 * 같은 몬스터라도 스폰 포인트의 등장 방식에 따라 순찰 여부가 다르다. Idle 상태가 이 조건으로
 * 순찰과 제자리 대기 중 하나를 고른다. StateTree는 영역이 어디서 왔는지 알 필요가 없다.
 */
USTRUCT(meta = (DisplayName = "Enemy Has Patrol Area", Category = "Enemy Framework"))
struct TEAMPROJECT_FIN_API FSTC_EnemyHasPatrolArea : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSTC_EnemyHasPatrolAreaInstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
