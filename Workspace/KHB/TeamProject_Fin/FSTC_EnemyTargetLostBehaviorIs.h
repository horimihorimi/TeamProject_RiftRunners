#pragma once

#include "CoreMinimal.h"
#include "GameCore/Enemy/Data/EnemyDataAssets.h"
#include "StateTreeConditionBase.h"
#include "FSTC_EnemyTargetLostBehaviorIs.generated.h"

USTRUCT()
struct FSTC_EnemyTargetLostBehaviorIsInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	EEnemyTargetLostBehavior Behavior = EEnemyTargetLostBehavior::StopAndSearch;
};

/**
 * 이 적의 타깃 상실 탐색 방식이 지정한 값인지 판정합니다.
 *
 * LostTarget 상태의 자식들이 이 조건으로 갈린다. 탐색 방식은 배치마다 달라서
 * 캐릭터가 들고 있고, StateTree 한 장을 모든 일반 적이 공유할 수 있게 한다.
 */
USTRUCT(meta = (DisplayName = "Enemy Target Lost Behavior Is", Category = "Enemy Framework"))
struct TEAMPROJECT_FIN_API FSTC_EnemyTargetLostBehaviorIs : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSTC_EnemyTargetLostBehaviorIsInstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
