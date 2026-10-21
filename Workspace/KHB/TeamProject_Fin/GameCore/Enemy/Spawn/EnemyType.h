#pragma once

#include "CoreMinimal.h"

#include "EnemyType.generated.h"

/*
 * 스폰 포인트가 풀에 요청할 때 쓰는 몹 종류.
 * 실제 BP 클래스는 UEnemySpawnTable이 이 값으로 찾는다. 종류를 늘리면 테이블에도 행을 추가한다
 */
UENUM(BlueprintType)
enum class EEnemyType : uint8
{
	Melee	UMETA(DisplayName = "근접"),
	Range	UMETA(DisplayName = "원거리"),
	Elite	UMETA(DisplayName = "엘리트")
};
