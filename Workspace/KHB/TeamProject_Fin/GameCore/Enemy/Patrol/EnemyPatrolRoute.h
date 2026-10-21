#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EnemyPatrolRoute.generated.h"

/**
 * 순찰 경로를 제공하는 쪽의 계약입니다.
 *
 * 경로를 어떻게 만드는지는 이 계약을 구현하는 쪽이 정한다. 맵 모듈 BP 안에 타겟 포인트를
 * 여러 개 두고 관리해도 되고, 나중에 스플라인으로 바꿔도 순찰 코드는 그대로다.
 * 순찰 순서(왕복인지 순환인지)를 계산하는 일은 코드가 맡고, 여기서는 지점과 규칙만 알려 준다.
 *
 * PCG로 조합되는 맵에서는 경로가 모듈 BP의 자식이라 월드 좌표가 매번 달라진다.
 * 그래서 지점은 저장된 좌표가 아니라 호출 시점의 월드 좌표로 돌려줘야 한다.
 */
UINTERFACE(BlueprintType, MinimalAPI)
class UEnemyPatrolRoute : public UInterface
{
	GENERATED_BODY()
};

class TEAMPROJECT_FIN_API IEnemyPatrolRoute
{
	GENERATED_BODY()

public:
	/** 순찰 지점들의 월드 좌표. 배열 순서가 곧 순찰 순서다. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Enemy|Patrol")
	void GetPatrolPoints(TArray<FVector>& OutPoints) const;
	virtual void GetPatrolPoints_Implementation(TArray<FVector>& OutPoints) const {}

	/** 해당 지점에 도착했을 때 머무를 시간(초). 0이면 멈추지 않고 다음 지점으로 간다. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Enemy|Patrol")
	float GetWaitTimeAtPoint(int32 PointIndex) const;
	virtual float GetWaitTimeAtPoint_Implementation(int32 PointIndex) const { return 0.0f; }

	/*
	 * 참이면 1→2→3→2→1처럼 끝에서 되돌아오고, 거짓이면 1→2→3→1로 순환한다.
	 *
	 * 되돌아오는 경로는 왕복이 자연스럽고, 순환 경로는 끝과 시작이 이어져 있을 때 쓴다.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Enemy|Patrol")
	bool IsPingPongRoute() const;
	virtual bool IsPingPongRoute_Implementation() const { return true; }
};
