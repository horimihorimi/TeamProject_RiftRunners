#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "PatrolArea.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UPatrolArea : public UInterface
{
	GENERATED_BODY()
};

/*
 * 순찰 영역의 계약. 순찰 AI는 영역을 누가 주는지 모르고 이 값만 읽는다.
 * PCG로 조립되는 맵은 매번 좌표가 달라서, 중심은 저장된 값이 아니라 호출 시점의 월드 좌표다
 */
class TEAMPROJECT_FIN_API IPatrolArea
{
	GENERATED_BODY()

public:
	virtual FVector GetPatrolCenter() const = 0;

	// 목적지는 중심에서 이 반경 안에서만 뽑는다
	virtual float GetPatrolRadius() const = 0;

	/*
	 * 한 번 이동할 때 현재 위치에서 최소 이만큼은 떨어진 곳으로 간다.
	 * 없으면 반경 안에서 1cm 옆이 뽑혀 제자리에서 도착-대기만 반복할 수 있다
	 */
	virtual float GetMinMoveDistance() const = 0;

	// 목적지에 닿은 뒤 머무를 시간. 호출마다 새로 뽑는다
	virtual float GetRandomWaitTime() const = 0;
};
