#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StateTreeTaskBase.h"
#include "FSTT_EnemyIdleMovement.generated.h"

USTRUCT()
struct FSTT_EnemyIdleMovementInstanceData
{
	GENERATED_BODY()

	/*
	 * 켜면 끝내도 되는 시점 이후에도, 미리 뽑아 둔 다음 패턴이 지금 실행 가능해질 때까지 기다린다.
	 *
	 * 끄면 이동 컴포넌트가 끝내도 된다고 하는 즉시 성공한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bWaitForPendingPattern = true;

	/** 끝내도 되는 시점 이후 다음 패턴을 더 기다릴 최대 시간(초). 넘기면 새로 고르도록 넘어갑니다. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0"))
	float MaxExtraWaitDuration = 3.0f;

	UPROPERTY(Transient)
	TObjectPtr<UActorComponent> MovementExecutor = nullptr;

	/** 끝내도 되는 상태가 된 월드 시간. 음수면 아직 그 시점이 오지 않았다. */
	double FinishableSinceTime = -1.0;
};

/**
 * 패턴과 패턴 사이의 기본 움직임을 몬스터별 IEnemyPatternMovement 구현체에 위임합니다.
 *
 * 어떤 움직임인지(보스 공전, 비행 순항 등)와 언제 끝내도 되는지는 구현체가 정한다.
 * 이 태스크는 끝내도 되는 시점 이후 다음 패턴이 준비됐는지만 보고 상태를 끝낸다.
 */
USTRUCT(meta = (DisplayName = "Enemy Idle Movement", Category = "Enemy Framework"))
struct TEAMPROJECT_FIN_API FSTT_EnemyIdleMovement : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSTT_EnemyIdleMovementInstanceData;

	FSTT_EnemyIdleMovement();

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
