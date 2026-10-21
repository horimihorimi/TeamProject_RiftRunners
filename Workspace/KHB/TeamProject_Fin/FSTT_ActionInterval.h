#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "FSTT_ActionInterval.generated.h"

USTRUCT()
struct FSTT_ActionIntervalInstanceData
{
	GENERATED_BODY()

	/** 이번 진입에서 선택할 최소 행동 간격. */
	UPROPERTY(EditAnywhere, Category = "Interval", meta = (ClampMin = "0.0"))
	float MinInterval = 2.0f;

	/** 이번 진입에서 선택할 최대 행동 간격. Min과 같으면 고정 간격이다. */
	UPROPERTY(EditAnywhere, Category = "Interval", meta = (ClampMin = "0.0"))
	float MaxInterval = 3.0f;

	// 런타임 데이터
	float ResolvedInterval = 0.0f;
	float ElapsedTime = 0.0f;
};

/**
 * 다음 행동을 실행하기 전까지의 공통 간격을 담당한다.
 * 진입할 때 MinInterval~MaxInterval 중 하나를 선택하고,
 * 시간이 지나면 Succeeded를 반환한다.
 */
USTRUCT(meta = (DisplayName = "Action Interval", Category = "Common"))
struct TEAMPROJECT_FIN_API FSTT_ActionInterval : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSTT_ActionIntervalInstanceData;

	FSTT_ActionInterval();

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;

	virtual EStateTreeRunStatus Tick(
		FStateTreeExecutionContext& Context,
		float DeltaTime) const override;
};
