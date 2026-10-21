#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "Poolable.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UPoolable : public UInterface
{
	GENERATED_BODY()
};

/*
 * 오브젝트 풀이 다루는 액터의 생명주기 계약.
 * 풀은 이 두 시점만 알리고, 무엇을 켜고 끌지는 구현하는 쪽이 정한다. 서버에서만 불린다
 */
class TEAMPROJECT_FIN_API IPoolable
{
	GENERATED_BODY()

public:
	// 풀에서 꺼낸 직후. 자리로 옮기고 새로 스폰된 것처럼 상태를 되돌린다. 아직 보이지는 않는다
	virtual void OnAcquired(const FTransform& SpawnTransform) = 0;

	// 풀로 돌아오기 직전. 숨기고 멈춘다. 파괴하지 않는다
	virtual void OnReleased() = 0;
};
