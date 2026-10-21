#pragma once

#include "CoreMinimal.h"

class AActor;
class ASpawnPoint;

/*
 * 그룹 ID가 같은 스폰 포인트를 찾는다. 섹터 조각 BP 안의 ChildActor 끼리는 에디터에서 인스턴스를 찍을 수 없어서
 * 전투 섹터가 이 ID로 자기 몹 자리를 찾는다. 찾는 범위는 호출한 액터가 놓인 곳이다.
 * 조각 BP의 ChildActor 면 그 조각 안, 레벨에 직접 놓였으면 같은 레벨 안. 다른 조각의 같은 ID와 섞이지 않는다
 */
class TEAMPROJECT_FIN_API FSpawnGroupLinker
{
public:
	static TArray<ASpawnPoint*> FindSpawnPoints(const AActor* Context, FName GroupId);
};
