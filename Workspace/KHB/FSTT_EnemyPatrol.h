#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "FSTT_EnemyPatrol.generated.h"

USTRUCT()
struct FSTT_EnemyPatrolInstanceData
{
	GENERATED_BODY()

	/** 목적지에 도착했다고 볼 거리(cm). 비행 적은 높이 오차까지 포함해 잰다. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "1.0"))
	float AcceptanceRadius = 120.0f;

	/** 최소 이동 거리를 만족하는 지점을 찾기 위해 다시 뽑는 횟수. 다 실패하면 뽑은 것 중 가장 먼 곳으로 간다. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "1"))
	int32 MaxPickAttempts = 8;

	/** 한 목적지로 가는 데 쓸 최대 시간(초). 넘기면 막힌 것으로 보고 새로 뽑는다. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "1.0"))
	float MaxLegDuration = 10.0f;

	/** 지금 향하는 목적지. */
	FVector Destination = FVector::ZeroVector;

	bool bHasDestination = false;

	/** 이번 목적지로 출발한 월드 시간. */
	double LegStartTime = 0.0;

	/** 대기가 끝나는 월드 시간. 음수면 대기 중이 아니다. */
	double WaitEndTime = -1.0;
};

/**
 * 순찰 영역 안을 무작위로 돌아다닙니다.
 *
 * 영역이 어디서 왔는지는 IPatrolArea 구현체가 알고, 이 태스크는 목적지 뽑기 → 이동 → 대기만 반복한다.
 * 목적지는 영역 중심 기준 반경 안의 NavMesh 지점이고, 현재 위치에서 최소 이동 거리 이상 떨어져야 한다.
 * 중심이 스폰 포인트에 고정되어 있어서, 전투하다 영역 밖으로 나간 적도 다음 목적지를 따라 영역으로 돌아온다.
 *
 * 이동은 이동 컴포넌트의 공통 진입점(UpdateMovementTowardLocation)에 맡긴다.
 * 지상 적은 NavMesh 경로로, 비행 적은 비행 이동으로 각자 알아서 간다.
 *
 * 순찰 영역이 없으면 진입하자마자 실패한다. Idle 상태는 이 실패를 받아 제자리 대기로 넘어간다.
 */
USTRUCT(meta = (DisplayName = "Enemy Patrol", Category = "Enemy Framework"))
struct TEAMPROJECT_FIN_API FSTT_EnemyPatrol : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSTT_EnemyPatrolInstanceData;

	FSTT_EnemyPatrol();

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
