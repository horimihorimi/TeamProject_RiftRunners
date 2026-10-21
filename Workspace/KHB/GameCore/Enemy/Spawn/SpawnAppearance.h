#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "SpawnAppearance.generated.h"

class AMonsterCharacter;

/*
 * 등장 방식 전략의 추상 베이스. 스폰 포인트 안에 Instanced로 들어가 드롭다운으로 고른다.
 * 스폰 포인트는 "어디에 무엇을"만 알고, 풀에서 받은 몹을 "어떻게" 내보낼지는 여기서 정한다.
 * 방식을 늘릴 때는 파생 클래스 하나만 추가한다.
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories)
class TEAMPROJECT_FIN_API USpawnAppearance : public UObject
{
	GENERATED_BODY()

public:
	// 풀에서 방금 꺼내 자리에 놓은 몹을 등장시킨다. InstigatorHint는 Execute를 부른 쪽: 플레이어 폰일 수도, 이벤트 액터일 수도 있다
	virtual void Apply(AMonsterCharacter* Monster, AActor* InstigatorHint) PURE_VIRTUAL(USpawnAppearance::Apply, );

	/*
	 * 누가 부르지 않아도 플레이어가 가까워지면 스스로 등장하는가. 기본은 호출될 때만.
	 * 참이면 플레이어가 멀어졌을 때 풀로 돌아갔다가 다시 가까워지면 새로 나온다
	 */
	virtual bool ShouldAutoExecute() const { return false; }

protected:
	// 이 방식을 담고 있는 스폰 포인트. 위치의 기준이다
	AActor* GetOwningSpawnPoint() const { return GetTypedOuter<AActor>(); }
};
